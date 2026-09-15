from pathlib import Path
import json
from datetime import datetime
from decimal import Decimal, ROUND_HALF_UP

from pyspark.sql import SparkSession
from pyspark.sql.functions import col, coalesce, count, countDistinct, explode, lit, row_number, sequence, sum as spark_sum, when, date_format, to_date
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
PILE_DATA_PATH = PROJECT_ROOT / "bigdata" / "out" / "ods_pile.csv"
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
PILE_SCHEMA = StructType(
    [StructField("id", LongType(), True), StructField("station_id", LongType(), True),
     StructField("code", StringType(), True), StructField("type", StringType(), True)]
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
        piles = read_csv(spark, PILE_DATA_PATH, PILE_SCHEMA)

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
        pile_types = piles.select(col("id").alias("pile_id"), col("type").alias("pile_type"))

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

        period_orders = valid_settled_orders.withColumn(
            "period",
            when((date_format(coalesce(col("start_time"), col("end_time")), "H") >= 0) & (date_format(coalesce(col("start_time"), col("end_time")), "H") < 6), "凌晨")
            .when(date_format(coalesce(col("start_time"), col("end_time")), "H") < 12, "上午")
            .when(date_format(coalesce(col("start_time"), col("end_time")), "H") < 18, "下午")
            .otherwise("晚间"),
        )
        period_counts = period_orders.groupBy("user_id", "period").agg(
            count("id").alias("period_orders")
        )
        period_ranked = period_counts.withColumn(
            "period_rank", row_number().over(
                Window.partitionBy("user_id").orderBy(col("period_orders").desc(), col("period").asc())
            )
        )
        preferred_period = period_ranked.filter(col("period_rank") == 1).select(
            "user_id", col("period").alias("preferred_period")
        )
        pile_counts = period_orders.join(pile_types, "pile_id").groupBy("user_id", "pile_type").agg(
            count("id").alias("pile_orders")
        )
        pile_ranked = pile_counts.withColumn(
            "pile_rank", row_number().over(
                Window.partitionBy("user_id").orderBy(col("pile_orders").desc(), col("pile_type").asc())
            )
        )
        preferred_pile = pile_ranked.filter(col("pile_rank") == 1).select(
            "user_id", col("pile_type").alias("preferred_pile_type")
        )
        behavior_orders = valid_settled_orders.withColumn(
            "behavior_time", coalesce(col("start_time"), col("end_time"))
        ).join(pile_types, "pile_id", "left")
        behavior_user = behavior_orders.groupBy("user_id").agg(
            count("id").alias("behavior_total_orders"),
            countDistinct(when(col("pile_type") == "fast", col("id"))).alias("fast_orders"),
            countDistinct(
                when(
                    (col("behavior_time") >= lit("2026-08-14 00:00:00").cast("timestamp"))
                    & (col("behavior_time") < lit("2026-09-14 00:00:00").cast("timestamp")),
                    to_date("behavior_time"),
                )
            ).alias("active_days_30d"),
        )
        station_counts = behavior_orders.groupBy("user_id", "station_id").count()
        from pyspark.sql.functions import max as spark_max
        station_max_rows = station_counts.groupBy("user_id").agg(
            spark_max("count").alias("max_station_order_count")
        )
        behavior_user = behavior_user.join(station_max_rows, "user_id", "left")
        behavior_profiles = (
            ranked_metrics.join(behavior_user, "user_id", "left")
            .select(
                "user_id", "frequency_key", "orders_30d", "kwh_30d", "amount_30d",
                coalesce(col("behavior_total_orders"), lit(0)).alias("total_orders_behavior"),
                coalesce(col("fast_orders"), lit(0)).alias("fast_orders"),
                coalesce(col("active_days_30d"), lit(0)).alias("active_days_30d"),
                coalesce(col("max_station_order_count"), lit(0)).alias("max_station_order_count"),
            )
        )
        behavior_group_rows = behavior_profiles.groupBy("frequency_key").agg(
            spark_sum("orders_30d").alias("orders_sum"),
            spark_sum("kwh_30d").alias("kwh_sum"),
            spark_sum("amount_30d").alias("amount_sum"),
            spark_sum("active_days_30d").alias("active_sum"),
            spark_sum(when(col("total_orders_behavior") > 0, col("fast_orders") / col("total_orders_behavior")).otherwise(lit(0.0))).alias("fast_sum"),
            spark_sum(when(col("total_orders_behavior") > 0, col("max_station_order_count") / col("total_orders_behavior")).otherwise(lit(0.0))).alias("focus_sum"),
            count("user_id").alias("group_count"),
        )
        behavior_raw = {}
        for row in behavior_group_rows.collect():
            n = row["group_count"]
            behavior_raw[row["frequency_key"]] = {
                "orders": float(row["orders_sum"] or 0) / n,
                "kwh": float(row["kwh_sum"] or 0) / n,
                "amount": float(row["amount_sum"] or 0) / n,
                "active": float(row["active_sum"] or 0) / n,
                "fast": float(row["fast_sum"] or 0) / n,
                "focus": float(row["focus_sum"] or 0) / n,
            }
        behavior_dims = ("orders", "kwh", "amount", "active", "fast", "focus")
        behavior_keys = ("high", "medium", "low")
        behavior_max = {}
        for dim in behavior_dims:
            max_raw = max(
                (
                    behavior_raw[key].get(dim, 0)
                    for key in behavior_keys
                    if key in behavior_raw
                ),
                default=0,
            )
            behavior_max[dim] = 1.0 if max_raw == 0 else max_raw
        behavior_names = {"high": "高频用户", "medium": "中频用户", "low": "低频用户"}
        behavior_comparison = [
            {
                "name": behavior_names[key],
                "group": behavior_names[key],
                "values": [round(min(100.0, behavior_raw[key][dim] / behavior_max[dim] * 100.0), 1) for dim in behavior_dims],
            }
            for key in behavior_keys if key in behavior_raw
        ]
        print(f"behavior comparison: {behavior_comparison}")
        enriched = (
            users.join(ranked_metrics, "user_id", "left")
            .join(preferred_period, "user_id", "left")
            .join(preferred_pile, "user_id", "left")
            .select("*", coalesce(col("preferred_pile_type"), lit("mixed")).alias("preferred_pile_type_final"))
        )
        records = []
        for row in enriched.collect():
            item = row.asDict()
            item["nickname"] = item.get("nickname") or ""
            phone = str(item.get("phone") or "")
            item["phone_masked"] = phone[:3] + "****" + phone[-4:] if len(phone) >= 7 else "****"
            item.pop("phone", None)
            item["frequency_level"] = {"high": "高频", "medium": "中频", "low": "低频", "inactive": "沉默"}[item["frequency_key"]]
            item["value_level"] = {"high": "高价值", "medium": "中价值", "low": "低价值"}[item["value_key"]]
            item["preferred_pile_type"] = {"fast": "快充", "slow": "慢充", "mixed": "混合"}[item.pop("preferred_pile_type_final")]
            item["preferred_period"] = item.get("preferred_period") or "--"
            item["last_active_at"] = item["last_active_at"].strftime("%Y-%m-%d %H:%M:%S") if item.get("last_active_at") else "--"
            for key, value in list(item.items()):
                if isinstance(value, datetime):
                    item[key] = value.strftime("%Y-%m-%d %H:%M:%S")
            for key in ("total_kwh", "total_amount", "kwh_30d", "amount_30d"):
                item[key] = float(item[key] or 0)
            records.append(item)
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        with (OUTPUT_DIR / "users.json").open("w", encoding="utf-8") as handle:
            json.dump(records, handle, ensure_ascii=False, indent=2)
        print(f"users.json rows: {len(records)}")
        if len(records) != user_count:
            raise ValueError(f"expected {user_count} users.json rows, got {len(records)}")

        frequency_distribution_dashboard = [
            {"name": label, "value": frequency_distribution.get(key, 0), "level": key}
            for key, label in (("high", "高频"), ("medium", "中频"), ("low", "低频"), ("inactive", "沉默"))
        ]
        value_distribution_dashboard = [
            {"name": label, "value": value_distribution.get(key, 0), "level": key}
            for key, label in (("high", "高价值"), ("medium", "中价值"), ("low", "低价值"))
        ]
        attribute_distribution_dashboard = {
            "age": [
                {"name": row["age_group"] or "未知", "value": row["count"]}
                for row in users.groupBy("age_group").count().collect()
            ],
            "city": [
                {"name": row["city"] or "未知", "value": row["count"]}
                for row in users.groupBy("city").count().collect()
            ],
            "source": [
                {"name": row["registration_source"] or "未知", "value": row["count"]}
                for row in users.groupBy("registration_source").count().collect()
            ],
        }
        period_counts_dashboard = {
            row["period"]: row["count"]
            for row in period_orders.groupBy("period").count().collect()
        }
        period_total = sum(period_counts_dashboard.values())
        period_distribution_dashboard = [
            {
                "name": period,
                "value": round(period_counts_dashboard.get(period, 0) * 100 / period_total, 1)
                if period_total
                else 0.0,
            }
            for period in ("凌晨", "上午", "下午", "晚间")
        ]
        new_users_30d = users.filter(
            (col("created_at") >= lit("2026-08-14 00:00:00").cast("timestamp"))
            & (col("created_at") < lit("2026-09-14 00:00:00").cast("timestamp"))
        ).count()
        overview_dashboard = {
            "total_users": user_count,
            "inactive_users_30d": frequency_distribution.get("inactive", 0),
            "active_users_30d": user_count - frequency_distribution.get("inactive", 0),
            "new_users_30d": new_users_30d,
            "total_orders": valid_settled_count,
            "total_kwh": float(valid_settled_orders.agg(spark_sum("kwh")).first()[0] or 0),
            "total_amount": float(valid_settled_orders.agg(spark_sum("amount")).first()[0] or 0),
        }
        print(f"dashboard overview: {overview_dashboard}")
        print(f"dashboard frequency distribution: {frequency_distribution_dashboard}")
        print(f"dashboard value distribution: {value_distribution_dashboard}")
        print(f"dashboard period distribution: {period_distribution_dashboard}")

        station_preferences = [
            {"station_id": row["station_id"], "name": row["name"], "user_count": row["user_count"]}
            for row in valid_settled_orders.groupBy("station_id")
            .agg(countDistinct("user_id").alias("user_count"))
            .join(stations.select(col("id").alias("station_id"), "name"), "station_id", "inner")
            .orderBy(col("user_count").desc(), col("station_id").asc())
            .limit(6)
            .collect()
        ]
        daily_orders = valid_settled_orders.withColumn("date", col("end_time").cast("date"))
        daily_facts = daily_orders.groupBy("date").agg(
            countDistinct("user_id").alias("active_users"),
            spark_sum("amount").alias("amount"),
        )
        daily_new_users = users.withColumn("date", col("created_at").cast("date")).groupBy("date").count().withColumnRenamed("count", "new_users")
        daily_facts = daily_facts.join(daily_new_users, "date", "full")
        calendar = spark.range(1).select(
            explode(sequence(lit("2026-06-16").cast("date"), lit("2026-09-13").cast("date"))).alias("date")
        )
        daily_facts = calendar.join(daily_facts, "date", "left").select(
            "date", coalesce(col("active_users"), lit(0)).alias("active_users"),
            coalesce(col("new_users"), lit(0)).alias("new_users"),
            coalesce(col("amount"), lit(0)).alias("amount"),
        )
        daily_rows = [
            {"date": str(row["date"]), "active_users": row["active_users"], "new_users": row["new_users"], "amount": float(Decimal(str(row["amount"] or 0)).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP))}
            for row in daily_facts.orderBy("date").collect()
        ]
        trends = daily_rows[-30:]
        trends_7d = daily_rows[-7:]
        trends_90d = daily_rows[-90:]
        print(f"station TOP6: {station_preferences}")
        print(f"trend lengths: trends={len(trends)}, trends_7d={len(trends_7d)}, trends_90d={len(trends_90d)}")
        if daily_rows:
            print(f"trend date range: {daily_rows[0]['date']} .. {daily_rows[-1]['date']}")
        insights = []
        inactive_pct = round(
            frequency_distribution.get("inactive", 0) * 100.0 / user_count, 1
        ) if user_count else 0.0
        if inactive_pct >= 15:
            insights.append({
                "icon": "⚠",
                "title": "建议开展沉默用户唤醒",
                "content": "可针对近30天未充电用户发放限时优惠券。",
                "reason": f"沉默用户占比{inactive_pct}%", "priority": 1,
            })
        evening_pct = next(
            (item["value"] for item in period_distribution_dashboard if item["name"] == "晚间"),
            0.0,
        )
        if evening_pct >= 35:
            insights.append({
                "icon": "◈",
                "title": "引导晚高峰用户错峰充电",
                "content": "建议将部分晚间订单引导至18点前。",
                "reason": f"晚间充电订单占比{evening_pct}%", "priority": 2,
            })
        high_amount = float(
            ranked_metrics.filter(col("value_key") == "high")
            .agg(spark_sum("total_amount"))
            .first()[0] or 0
        )
        all_amount = overview_dashboard["total_amount"]
        high_amount_pct = round(high_amount * 100.0 / all_amount, 1) if all_amount > 0 else 0.0
        if high_amount_pct >= 40:
            insights.append({
                "icon": "★",
                "title": "加强高价值用户维护",
                "content": "为高价值用户提供预约及积分权益。",
                "reason": f"高价值用户贡献消费额{high_amount_pct}%", "priority": 2,
            })
        insights.sort(key=lambda item: (item["priority"], item["title"]))
        print(f"insights: {insights}")
        dashboard_payload = {
            "behavior_comparison": behavior_comparison,
            "overview": overview_dashboard,
            "frequency_distribution": frequency_distribution_dashboard,
            "value_distribution": value_distribution_dashboard,
            "attribute_distribution": attribute_distribution_dashboard,
            "period_distribution": period_distribution_dashboard,
            "station_preferences": station_preferences,
            "trends": trends,
            "trends_7d": trends_7d,
            "trends_90d": trends_90d,
            "users": records[:10],
            "user_total": user_count,
            "insights": insights,
        }
        with (OUTPUT_DIR / "dashboard.json").open("w", encoding="utf-8") as handle:
            json.dump(dashboard_payload, handle, ensure_ascii=False, indent=2)
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
