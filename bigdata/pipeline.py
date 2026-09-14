#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""充电大数据流水线（NO.113–118）。

默认读邓雅心的 ODS（`bigdata/out/ods_*.csv`），探查数字对 `dq_expected.json`。
清洗口径跟她的契约：NO.114 清单丢弃空 start / 空或负 kwh / 时间颠倒 / 孤儿，
重复 order_no 留最新；SOC_RANGE、STATUS_CONFLICT 留在 DWD。禁止用 dq_tag 当过滤条件。

  python3 bigdata/gen_ods.py --orders
  source ~/.hadoop_env.sh
  spark-submit bigdata/pipeline.py --stage qa
  spark-submit bigdata/pipeline.py --stage clean
  spark-submit bigdata/pipeline.py --stage dws
  spark-submit bigdata/pipeline.py --stage qa --ods hdfs   # 已 put 到 HDFS 时

后续 stage：ads / train（尚未实现）。
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from decimal import Decimal
from pathlib import Path

os.environ.setdefault("PYSPARK_PYTHON", sys.executable)
os.environ.setdefault("PYSPARK_DRIVER_PYTHON", sys.executable)

from pyspark.sql import SparkSession, functions as F
from pyspark.sql.window import Window

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "bigdata" / "out"
MINI_DIR = ROOT / "bigdata" / "ods"
WORK_DIR = ROOT / "bigdata" / "work"
CONTRACT_PATH = ROOT / "bigdata" / "dq_expected.json"
HDFS_ODS = "hdfs://localhost:8020/user/charging/ods"

# 与 check_ods_hdfs.py 同一套显式 schema：空字段当 null，不用 inferSchema。
# CSV reader 在给了 schema 时按位置映射，列名必须与 SHOW CREATE TABLE 一致。
CHARGE_ORDER_SCHEMA = """
    id long, order_no string, user_id long, station_id long, pile_id long,
    status string, unit_price decimal(6,2), start_soc decimal(5,2),
    battery_capacity_kwh decimal(6,2), target_soc decimal(5,2),
    reserve_time timestamp, start_time timestamp, end_time timestamp,
    duration_seconds int, kwh decimal(10,2), amount decimal(10,2),
    pay_request_id string, created_at timestamp, updated_at timestamp,
    dq_tag string
"""
STATION_SCHEMA = """
    id long, station_code string, name string, address string,
    longitude decimal(10,6), latitude decimal(10,6), price decimal(6,2),
    enabled tinyint, created_at timestamp, updated_at timestamp
"""
PILE_SCHEMA = """
    id long, station_id long, code string, type string,
    power_kw decimal(6,2), status string, current_user_id long,
    last_online_at timestamp, enabled tinyint, total_count int, total_hours decimal(10,2)
"""

LOCAL_FILES = {
    "order": OUT_DIR / "ods_charge_order.csv",
    "station": OUT_DIR / "ods_station.csv",
    "pile": OUT_DIR / "ods_pile.csv",
}
HDFS_FILES = {
    "order": HDFS_ODS + "/charge_order/ods_charge_order.csv",
    "station": HDFS_ODS + "/station/ods_station.csv",
    "pile": HDFS_ODS + "/pile/ods_pile.csv",
}
MINI_FILES = {
    "order": MINI_DIR / "charge_order.csv",
    "station": MINI_DIR / "station.csv",
    "pile": MINI_DIR / "pile.csv",
}

# spark-submit 入口里赋值；load_ods / DWS 共用。
ODS_KIND = "auto"


def spark_session(app: str, use_hdfs: bool = False) -> SparkSession:
    builder = (
        SparkSession.builder.appName(app)
        .master("local[*]")
        .config("spark.sql.session.timeZone", "Asia/Shanghai")
        .config("spark.ui.port", "4040")
    )
    # 只在读 HDFS 时指定 defaultFS，避免 NameNode 没起时本地跑卡住
    if use_hdfs:
        builder = builder.config("spark.hadoop.fs.defaultFS", "hdfs://localhost:8020")
    spark = builder.getOrCreate()
    spark.sparkContext.setLogLevel("WARN")
    return spark


def file_uri(path: Path) -> str:
    return path.resolve().as_uri()


def local_out_ready() -> bool:
    return all(p.is_file() for p in LOCAL_FILES.values())


def resolve_ods(kind: str) -> tuple[str, dict]:
    """返回 (实际类型, {order,station,pile} URI)。"""
    if kind == "auto":
        if local_out_ready():
            kind = "local"
        else:
            raise FileNotFoundError(
                "找不到邓雅心的 ODS（bigdata/out/ods_*.csv）。\n"
                "  先跑：python3 bigdata/gen_ods.py --orders\n"
                "  已上传 HDFS 则：spark-submit bigdata/pipeline.py --stage qa --ods hdfs"
            )
    if kind == "local":
        missing = [str(p) for p in LOCAL_FILES.values() if not p.is_file()]
        if missing:
            raise FileNotFoundError(
                "本地 ODS 不完整，缺：\n  "
                + "\n  ".join(missing)
                + "\n先跑：python3 bigdata/gen_ods.py --orders"
            )
        return kind, {k: file_uri(p) for k, p in LOCAL_FILES.items()}
    if kind == "hdfs":
        return kind, dict(HDFS_FILES)
    if kind == "mini":
        missing = [str(p) for p in MINI_FILES.values() if not p.is_file()]
        if missing:
            raise FileNotFoundError("迷你 ODS 缺失: " + ", ".join(missing))
        return kind, {k: file_uri(p) for k, p in MINI_FILES.items()}
    raise ValueError("未知 --ods: " + kind)


def read_ods_csv(spark: SparkSession, uri: str, schema: str | None):
    reader = (
        spark.read.option("header", True)
        .option("nullValue", "")
        .option("emptyValue", "")
        .option("encoding", "UTF-8")
    )
    if schema:
        reader = reader.schema(schema)
    return reader.csv(uri)


def read_csv(spark: SparkSession, path: Path):
    """DWD/DWS 工作区产物：无显式 schema，按字符串读。"""
    if not path.is_file():
        raise FileNotFoundError(path)
    return (
        spark.read.option("header", True)
        .option("nullValue", "")
        .option("emptyValue", "")
        .option("encoding", "UTF-8")
        .csv(file_uri(path))
    )


def write_csv(df, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    tmp = dest / "_spark"
    if tmp.exists():
        for p in tmp.rglob("*"):
            if p.is_file():
                p.unlink()
    df.coalesce(1).write.mode("overwrite").option("header", True).csv(file_uri(tmp))
    parts = list(tmp.glob("part-*.csv"))
    if not parts:
        raise RuntimeError("Spark 未写出 part-*.csv: " + str(tmp))
    out = dest / (dest.name + ".csv")
    out.write_bytes(parts[0].read_bytes())


def load_ods(spark: SparkSession):
    kind, uris = resolve_ods(ODS_KIND)
    use_schema = kind != "mini"
    orders = read_ods_csv(
        spark, uris["order"], CHARGE_ORDER_SCHEMA if use_schema else None
    )
    stations = read_ods_csv(
        spark, uris["station"], STATION_SCHEMA if use_schema else None
    )
    piles = read_ods_csv(spark, uris["pile"], PILE_SCHEMA if use_schema else None)
    print("ODS source:", kind)
    print("  order  ", uris["order"])
    print("  station", uris["station"])
    print("  pile   ", uris["pile"])
    return kind, orders, stations, piles


def load_contract() -> dict | None:
    if not CONTRACT_PATH.is_file():
        return None
    return json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))


def dim_keys(df, pk_alias: str):
    return df.select(F.col("id").alias(pk_alias))


def apply_clean(orders, stations, piles):
    """NO.114：按规则清洗。不用 dq_tag。STATUS_CONFLICT / SOC_RANGE 保留。"""
    kept = orders.filter(
        F.col("start_time").isNotNull()
        & F.col("kwh").isNotNull()
        & (F.col("kwh") >= 0)
        & ~(F.col("end_time") < F.col("start_time"))
    )
    st = dim_keys(stations, "station_pk")
    pl = dim_keys(piles, "pile_pk")
    kept = kept.join(st, kept["station_id"] == st["station_pk"], "left_semi")
    kept = kept.join(pl, kept["pile_id"] == pl["pile_pk"], "left_semi")
    w = Window.partitionBy("order_no").orderBy(
        F.col("updated_at").desc(), F.col("created_at").desc()
    )
    return (
        kept.withColumn("_rn", F.row_number().over(w))
        .filter(F.col("_rn") == 1)
        .drop("_rn")
    )


def probe(orders, stations, piles) -> dict:
    st = dim_keys(stations, "station_pk")
    pl = dim_keys(piles, "pile_pk")
    dup_df = orders.groupBy("order_no").count().filter(F.col("count") > 1)
    dup_extra = dup_df.agg(F.coalesce(F.sum(F.col("count") - 1), F.lit(0))).collect()[0][0]
    return {
        "ods_rows": orders.count(),
        "null_start_time": orders.filter(F.col("start_time").isNull()).count(),
        "null_kwh": orders.filter(F.col("kwh").isNull()).count(),
        "negative_kwh": orders.filter(F.col("kwh") < 0).count(),
        "soc_range": orders.filter(
            (F.col("target_soc") < 0) | (F.col("target_soc") > 100)
        ).count(),
        # 时间颠倒用严格 < ；写成 <= 会把 STATUS_CONFLICT 数进去
        "time_reversed": orders.filter(
            F.col("start_time").isNotNull()
            & F.col("end_time").isNotNull()
            & (F.col("end_time") < F.col("start_time"))
        ).count(),
        "dup_order_no": dup_df.count(),
        "dup_extra_rows": int(dup_extra or 0),
        "orphan_station": orders.join(
            st, orders["station_id"] == st["station_pk"], "left_anti"
        ).count(),
        "orphan_pile": orders.join(
            pl, orders["pile_id"] == pl["pile_pk"], "left_anti"
        ).count(),
        "status_conflict": orders.filter(
            (F.col("status") == "settled")
            & (F.col("duration_seconds") == 0)
            & (F.col("kwh") > 0)
        ).count(),
    }


def compare_probe(got: dict, expected: dict) -> list[str]:
    want = expected.get("expected_probe", {})
    bad = []
    print("\n对契约 expected_probe：")
    for key in (
        "ods_rows",
        "null_start_time",
        "null_kwh",
        "negative_kwh",
        "soc_range",
        "time_reversed",
        "dup_order_no",
        "dup_extra_rows",
        "orphan_station",
        "orphan_pile",
        "status_conflict",
    ):
        if key not in want:
            continue
        g, w = got.get(key), want[key]
        ok = g == w
        print("  %s %s  got=%s  want=%s" % ("✅" if ok else "❌", key, g, w))
        if not ok:
            bad.append(key)
    if bad:
        print(
            "  提示：null_start_time 含 reserved 等天然空 start，"
            "不要当成注入脏行数。"
        )
    return bad


def write_report(report: dict) -> Path:
    qa_dir = WORK_DIR / "qa"
    qa_dir.mkdir(parents=True, exist_ok=True)
    report_path = qa_dir / "report.json"
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return report_path


def stage_qa(spark: SparkSession) -> int:
    kind, orders, stations, piles = load_ods(spark)
    orders = orders.cache()
    got = probe(orders, stations, piles)
    report = {
        "generated_at": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
        "ods_source": kind,
        **got,
        # 给大屏 mock 的旧键名
        "end_before_start": got["time_reversed"],
        "status_contradiction": got["status_conflict"],
        "note": "NO.113 探查；数字应对齐 bigdata/dq_expected.json 的 expected_probe",
    }
    rc = 0
    contract = load_contract() if kind != "mini" else None
    if contract:
        bad = compare_probe(got, contract)
        report["contract_ok"] = not bad
        if bad:
            rc = 1
    elif kind == "mini":
        report["note"] = "迷你 ODS 烟测，不对 dq_expected.json"
        print("迷你 ODS 烟测，跳过契约比对")
    path = write_report(report)
    print("QA report ->", path)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return rc


def stage_clean(spark: SparkSession) -> int:
    kind, orders, stations, piles = load_ods(spark)
    before = orders.count()
    dwd = apply_clean(orders, stations, piles).drop("dq_tag")
    after = dwd.count()
    kwh_total = dwd.agg(F.sum(F.col("kwh").cast("double"))).collect()[0][0]
    kwh_total = float(kwh_total or 0.0)
    dest = WORK_DIR / "dwd" / "charge_order"
    write_csv(dwd, dest)
    print("DWD rows: %s -> %s  file: %s" % (before, after, dest / "charge_order.csv"))
    print("DWD kwh total:", round(kwh_total, 2))

    rc = 0
    contract = load_contract() if kind != "mini" else None
    if contract:
        want_n = contract["derived"]["dwd_rows"]
        want_k = Decimal(contract["derived"]["dwd_kwh_total"])
        n_ok = after == want_n
        k_ok = abs(Decimal(str(round(kwh_total, 2))) - want_k) < Decimal("0.5")
        print("  %s DWD 行数 %s  (契约 %s)" % ("✅" if n_ok else "❌", after, want_n))
        print("  %s DWD kwh %s  (契约 %s)" % ("✅" if k_ok else "❌", round(kwh_total, 2), want_k))
        if not n_ok or not k_ok:
            rc = 1
            print(
                "  口径：SOC_RANGE 与 STATUS_CONFLICT 必须留在 DWD；"
                "丢掉 STATUS_CONFLICT 会变成 %s 行 / %s 度"
                % (
                    contract["derived"]["dwd_rows_if_status_conflict_dropped"],
                    contract["derived"]["dwd_kwh_if_status_conflict_dropped"],
                )
            )
        report_path = WORK_DIR / "qa" / "report.json"
        report = {}
        if report_path.is_file():
            report = json.loads(report_path.read_text(encoding="utf-8"))
        report.update(
            {
                "dwd_rows": after,
                "dwd_kwh_total": round(kwh_total, 2),
                "dwd_contract_ok": n_ok and k_ok,
            }
        )
        write_report(report)
    elif after >= before:
        print("WARN: 清洗后行数未减少，请检查 ODS 脏数据是否读到")
    return rc


def dwd_path() -> Path:
    return WORK_DIR / "dwd" / "charge_order" / "charge_order.csv"


def stage_dws(spark: SparkSession) -> int:
    """NO.115：SparkSQL 把 DWD 订单拆成站×小时（DWS）。"""
    src = dwd_path()
    if not src.is_file():
        print("未找到 DWD，先跑 clean:", src)
        rc = stage_clean(spark)
        if rc != 0:
            print("WARN: clean 与契约不一致，仍继续做 DWS")
    if not src.is_file():
        raise FileNotFoundError(src)

    dwd = read_csv(spark, src)
    _, _, piles = load_ods(spark)
    piles.createOrReplaceTempView("ods_pile")
    dwd.createOrReplaceTempView("dwd_order")

    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW station_pile_cnt AS
        SELECT CAST(station_id AS BIGINT) AS station_id,
               COUNT(*) AS total_piles
        FROM ods_pile
        GROUP BY CAST(station_id AS BIGINT)
        """
    )

    # 半开区间 [start, end) 落到整点小时，电量按重叠秒数分摊
    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW order_hours AS
        SELECT
          o.order_no,
          CAST(o.station_id AS BIGINT) AS station_id,
          CAST(o.pile_id AS BIGINT) AS pile_id,
          o.start_ts,
          o.end_ts,
          o.duration_seconds,
          o.kwh,
          explode(
            sequence(
              date_trunc('HOUR', o.start_ts),
              CASE
                WHEN o.end_ts = date_trunc('HOUR', o.end_ts)
                  THEN date_trunc('HOUR', o.end_ts) - INTERVAL 1 HOUR
                ELSE date_trunc('HOUR', o.end_ts)
              END,
              INTERVAL 1 HOUR
            )
          ) AS hour_ts
        FROM (
          SELECT
            order_no,
            station_id,
            pile_id,
            to_timestamp(start_time) AS start_ts,
            to_timestamp(end_time) AS end_ts,
            CAST(duration_seconds AS INT) AS duration_seconds,
            CAST(kwh AS DOUBLE) AS kwh
          FROM dwd_order
          WHERE start_time IS NOT NULL
            AND end_time IS NOT NULL
            AND CAST(duration_seconds AS INT) > 0
            AND CAST(kwh AS DOUBLE) >= 0
        ) o
        WHERE o.end_ts > o.start_ts
        """
    )

    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW order_hour_share AS
        SELECT
          order_no,
          station_id,
          pile_id,
          hour_ts,
          CAST(
            kwh * greatest(
              unix_timestamp(least(end_ts, hour_ts + INTERVAL 1 HOUR))
              - unix_timestamp(greatest(start_ts, hour_ts)),
              0
            ) / duration_seconds
            AS DOUBLE
          ) AS kwh_share
        FROM order_hours
        """
    )

    # 默认窗口 8/15–9/13 内无节假日，is_holiday 多为 0
    dws = spark.sql(
        """
        SELECT
          s.station_id,
          date_format(s.hour_ts, 'yyyy-MM-dd HH:mm:ss') AS hour_ts,
          ROUND(SUM(s.kwh_share), 4) AS kwh,
          COUNT(DISTINCT s.order_no) AS order_count,
          CAST(
            least(COUNT(DISTINCT s.pile_id), first(p.total_piles)) AS INT
          ) AS busy_piles,
          CAST(
            first(p.total_piles) - least(COUNT(DISTINCT s.pile_id), first(p.total_piles))
            AS INT
          ) AS idle_piles,
          first(p.total_piles) AS total_piles,
          CAST(pmod(dayofweek(s.hour_ts) + 5, 7) AS INT) AS weekday,
          CAST(IF(pmod(dayofweek(s.hour_ts) + 5, 7) IN (5, 6), 1, 0) AS INT) AS is_weekend,
          CAST(
            IF(date_format(s.hour_ts, 'yyyy-MM-dd') IN (
              '2026-01-01','2026-05-01','2026-10-01','2026-10-02','2026-10-03',
              '2026-10-04','2026-10-05','2026-10-06','2026-10-07'
            ), 1, 0) AS INT
          ) AS is_holiday,
          CAST(hour(s.hour_ts) AS INT) AS hour_of_day
        FROM order_hour_share s
        JOIN station_pile_cnt p ON s.station_id = p.station_id
        GROUP BY s.station_id, s.hour_ts
        ORDER BY s.station_id, s.hour_ts
        """
    )
    dest = WORK_DIR / "dws" / "station_hour"
    write_csv(dws, dest)
    n = dws.count()
    print("DWS station_hour rows:", n, " file:", dest / "station_hour.csv")
    dws.show(20, truncate=False)
    if n == 0:
        raise RuntimeError("DWS 为空：检查 DWD 时间字段是否能被 to_timestamp 解析")
    return 0


def not_ready(stage: str) -> int:
    print("stage=%s 尚未实现。dws 通了之后再补 ADS 与 MLlib。" % stage)
    return 0


def main() -> int:
    global ODS_KIND
    parser = argparse.ArgumentParser(description="charging-system Spark 流水线")
    parser.add_argument(
        "--stage",
        required=True,
        choices=["qa", "clean", "dws", "ads", "train"],
        help="qa=探查 clean=清洗 dws/ads=SparkSQL train=MLlib",
    )
    parser.add_argument(
        "--ods",
        default="auto",
        choices=["auto", "local", "hdfs", "mini"],
        help="auto=有 bigdata/out 则用邓雅心本地 ODS；hdfs=读 /user/charging/ods；mini=16 行烟测",
    )
    args = parser.parse_args()
    ODS_KIND = args.ods

    spark = spark_session(
        "charging-pipeline-" + args.stage,
        use_hdfs=(args.ods == "hdfs"),
    )
    try:
        if args.stage == "qa":
            return stage_qa(spark)
        if args.stage == "clean":
            return stage_clean(spark)
        if args.stage == "dws":
            return stage_dws(spark)
        return not_ready(args.stage)
    finally:
        spark.stop()


if __name__ == "__main__":
    raise SystemExit(main())
