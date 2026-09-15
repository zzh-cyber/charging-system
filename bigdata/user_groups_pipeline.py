from pathlib import Path

from pyspark.sql import SparkSession
from pyspark.sql.functions import col, coalesce, count, lit, row_number, sum as spark_sum, when
from pyspark.sql.window import Window
from pyspark.sql.types import (
    DecimalType,
    IntegerType,
    LongType,
    StringType,
    StructField,
    StructType,
    TimestampType,
)


PROJECT_ROOT = Path(__file__).resolve().parent.parent
USER_DATA_PATH = PROJECT_ROOT / "bigdata" / "ods_user" / "ods_ug_user.csv"
ORDER_DATA_PATH = PROJECT_ROOT / "bigdata" / "ods_user" / "ods_ug_charge_order.csv"
STATION_DATA_PATH = PROJECT_ROOT / "bigdata" / "out" / "ods_station.csv"
OUTPUT_DIR = PROJECT_ROOT / "bigdata" / "work" / "ads" / "user_groups"

USER_SCHEMA = StructType(
    [
        StructField("user_id", LongType(), True),
        StructField("phone", StringType(), True),
        StructField("nickname", StringType(), True),
        StructField("gender", StringType(), True),
        StructField("age_group", StringType(), True),
        StructField("city", StringType(), True),
        StructField("vehicle_type", StringType(), True),
        StructField("registration_source", StringType(), True),
        StructField("created_at", TimestampType(), True),
        StructField("last_active_at", TimestampType(), True),
        StructField("status", StringType(), True),
    ]
)

ORDER_SCHEMA = StructType(
    [
        StructField("id", LongType(), True),
        StructField("order_no", StringType(), True),
        StructField("user_id", LongType(), True),
        StructField("station_id", LongType(), True),
        StructField("pile_id", LongType(), True),
        StructField("status", StringType(), True),
        StructField("unit_price", DecimalType(10, 2), True),
        StructField("start_soc", DecimalType(5, 2), True),
        StructField("battery_capacity_kwh", DecimalType(10, 2), True),
        StructField("target_soc", DecimalType(5, 2), True),
        StructField("reserve_time", TimestampType(), True),
        StructField("start_time", TimestampType(), True),
        StructField("end_time", TimestampType(), True),
        StructField("duration_seconds", IntegerType(), True),
        StructField("kwh", DecimalType(12, 2), True),
        StructField("amount", DecimalType(12, 2), True),
        StructField("pay_request_id", StringType(), True),
        StructField("created_at", TimestampType(), True),
        StructField("updated_at", TimestampType(), True),
        StructField("dq_tag", StringType(), True),
    ]
)

STATION_SCHEMA = StructType(
    [
        StructField("id", LongType(), True),
        StructField("station_code", StringType(), True),
        StructField("name", StringType(), True),
        StructField("address", StringType(), True),
        StructField("longitude", DecimalType(10, 6), True),
        StructField("latitude", DecimalType(9, 6), True),
        StructField("price", DecimalType(10, 2), True),
        StructField("enabled", IntegerType(), True),
        StructField("created_at", TimestampType(), True),
        StructField("updated_at", TimestampType(), True),
    ]
)


def read_csv(spark: SparkSession, path: Path, schema: StructType):
    return spark.read.option("header", True).schema(schema).csv(path.as_uri())


def print_null_counts(name: str, dataframe, fields: list[str]) -> None:
    counts = dataframe.agg(
        *[spark_sum(col(field).isNull().cast("int")).alias(field) for field in fields]
    ).first()
    print(f"{name} required-field null counts: {counts.asDict()}")


def main() -> None:
    spark = (
        SparkSession.builder.master("local[*]")
        .appName("user-groups-pipeline")
        .getOrCreate()
    )
    try:
        print(f"user input file: {USER_DATA_PATH}")
        print(f"order input file: {ORDER_DATA_PATH}")
        print(f"station input file: {STATION_DATA_PATH}")
        print(f"user groups output directory: {OUTPUT_DIR}")

        users = read_csv(spark, USER_DATA_PATH, USER_SCHEMA)
        orders = read_csv(spark, ORDER_DATA_PATH, ORDER_SCHEMA)
        stations = read_csv(spark, STATION_DATA_PATH, STATION_SCHEMA)

        datasets = (("user", users), ("order", orders), ("station", stations))
        for name, dataframe in datasets:
            print(f"{name} schema:")
            dataframe.printSchema()

        user_count = users.count()
        order_count = orders.count()
        station_count = stations.count()
        print(f"row counts: user={user_count}, order={order_count}, station={station_count}")

        if user_count != 11247:
            raise ValueError(f"expected 11247 user rows, got {user_count}")
        if order_count != 43866:
            raise ValueError(f"expected 43866 order rows, got {order_count}")

        print_null_counts("user", users, ["user_id"])
        print_null_counts("order", orders, ["id", "user_id", "station_id", "status"])
        print_null_counts("station", stations, ["id", "name"])

        settled_orders = orders.filter(col("status") == "settled")
        valid_user_ids = users.select("user_id").distinct()
        valid_station_ids = stations.select(col("id").alias("station_id")).distinct()

        orphan_user_orders = settled_orders.join(valid_user_ids, "user_id", "left_anti")
        invalid_station_orders = settled_orders.join(
            valid_station_ids, "station_id", "left_anti"
        )
        valid_settled_orders = (
            settled_orders.join(valid_user_ids, "user_id", "inner")
            .join(valid_station_ids, "station_id", "inner")
        )

        settled_count = settled_orders.count()
        orphan_user_count = orphan_user_orders.count()
        invalid_station_count = invalid_station_orders.count()
        valid_settled_count = valid_settled_orders.count()
        print(
            "order validity counts: "
            f"raw={order_count}, settled={settled_count}, "
            f"orphan_user_id={orphan_user_count}, "
            f"invalid_station_id={invalid_station_count}, "
            f"valid_settled={valid_settled_count}"
        )

        all_time_metrics = valid_settled_orders.groupBy("user_id").agg(
            count("id").alias("total_orders"),
            spark_sum("kwh").alias("total_kwh"),
            spark_sum("amount").alias("total_amount"),
        )
        metrics_30d = (
            valid_settled_orders.filter(
                (col("start_time") >= lit("2026-08-14 00:00:00").cast("timestamp"))
                & (col("start_time") < lit("2026-09-14 00:00:00").cast("timestamp"))
            )
            .groupBy("user_id")
            .agg(
                count("id").alias("orders_30d"),
                spark_sum("kwh").alias("kwh_30d"),
                spark_sum("amount").alias("amount_30d"),
            )
        )
        user_metrics = (
            users.select("user_id")
            .join(all_time_metrics, "user_id", "left")
            .join(metrics_30d, "user_id", "left")
            .select(
                "user_id",
                *[
                    coalesce(col(field), lit(0)).alias(field)
                    for field in (
                        "total_orders",
                        "total_kwh",
                        "total_amount",
                        "orders_30d",
                        "kwh_30d",
                        "amount_30d",
                    )
                ],
            )
        )
        user_metrics_count = user_metrics.count()
        zero_order_users = user_metrics.filter(col("total_orders") == 0).count()
        zero_30d_users = user_metrics.filter(col("orders_30d") == 0).count()
        print(
            "user metrics self-check: "
            f"rows={user_metrics_count}, "
            f"zero_total_order_users={zero_order_users}, "
            f"zero_30d_order_users={zero_30d_users}"
        )
        if user_metrics_count != user_count:
            raise ValueError(f"expected {user_count} user metric rows, got {user_metrics_count}")

        user_metrics = user_metrics.withColumn(
            "frequency_key",
            when(col("orders_30d") >= 8, "high")
            .when(col("orders_30d") >= 3, "medium")
            .when(col("orders_30d") >= 1, "low")
            .otherwise("inactive"),
        )
        frequency_distribution = {
            row["frequency_key"]: row["count"]
            for row in user_metrics.groupBy("frequency_key")
            .count()
            .collect()
        }
        print(f"frequency distribution: {frequency_distribution}")
        expected_frequency = {"high": 1349, "medium": 3149, "low": 3712, "inactive": 3037}
        if frequency_distribution != expected_frequency:
            print(
                "frequency distribution mismatch: "
                f"expected={expected_frequency}, actual={frequency_distribution}"
            )

        value_window = Window.orderBy(col("amount_30d").desc(), col("user_id").asc())
        ranked_metrics = user_metrics.withColumn("value_rank", row_number().over(value_window))
        ranked_metrics = ranked_metrics.withColumn(
            "value_key",
            when(col("value_rank") <= 2249, "high")
            .when(col("value_rank") <= 7872, "medium")
            .otherwise("low"),
        )
        value_distribution = {
            row["value_key"]: row["count"]
            for row in ranked_metrics.groupBy("value_key").count().collect()
        }
        print(f"value distribution: {value_distribution}")
        expected_value = {"high": 2249, "medium": 5623, "low": 3375}
        if value_distribution != expected_value:
            print(
                "value distribution mismatch: "
                f"expected={expected_value}, actual={value_distribution}"
            )
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
