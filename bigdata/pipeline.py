#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""充电大数据流水线（NO.113–118）。

默认读邓雅心的 ODS（`bigdata/out/ods_*.csv`），探查数字对 `dq_expected.json`。
清洗口径跟她的契约：NO.114 清单丢弃空 start / 空或负 kwh / 时间颠倒 / 孤儿，
重复 order_no 留最新；SOC_RANGE、STATUS_CONFLICT 留在 DWD。禁止用 dq_tag 当过滤条件。

  python3 bigdata/gen_ods.py --orders
  source ~/.hadoop_env.sh
  ./bigdata/hdfs_sync.sh                    # NO.118：冻结 ODS 上 HDFS 后 qa→train
  spark-submit --driver-memory 2g bigdata/pipeline.py --stage train
  spark-submit bigdata/pipeline.py --stage qa --ods hdfs   # 已 put 到 HDFS 时
"""
from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
from datetime import date, datetime, timedelta, timezone
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
        .config("spark.driver.memory", "2g")
        .config("spark.driver.maxResultSize", "1g")
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
    _, _, _, piles = load_ods(spark)
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
    try_hdfs_put(dest / "station_hour.csv", "/user/charging/dws/station_hour")
    return 0


def dws_path() -> Path:
    return WORK_DIR / "dws" / "station_hour" / "station_hour.csv"


def try_hdfs_put(local: Path, hdfs_dir: str) -> None:
    """NameNode 活着就把单个文件放到目录里。失败不挡本地结果。"""
    if not local.is_file():
        return
    try:
        ls = subprocess.run(
            ["hdfs", "dfs", "-ls", "/"],
            capture_output=True,
            timeout=20,
        )
        if ls.returncode != 0:
            print("HDFS 未就绪，跳过上传", hdfs_dir)
            return
        subprocess.run(
            ["hdfs", "dfs", "-mkdir", "-p", hdfs_dir],
            check=True,
            timeout=20,
        )
        subprocess.run(
            ["hdfs", "dfs", "-put", "-f", str(local), hdfs_dir + "/"],
            check=True,
            timeout=60,
        )
        print("HDFS <-", hdfs_dir + "/" + local.name)
    except (OSError, subprocess.SubprocessError) as exc:
        print("HDFS 上传跳过:", exc)


def congestion_of(idle: int, total: int, util: float) -> str:
    if total <= 0:
        return "mid"
    ratio = idle / float(total)
    if ratio >= 0.5:
        return "low"
    if ratio < 0.2 or util >= 80.0:
        return "high"
    return "mid"


def forecast_slot(kwh: float, idle: int, total: int) -> dict:
    util = 0.0 if total <= 0 else round((total - idle) * 100.0 / total, 1)
    util = max(0.0, min(100.0, util))
    idle = max(0, min(int(total), int(idle)))
    return {
        "kwh": round(float(kwh), 2),
        "idle": idle,
        "util": util,
        "is_peak": 1 if util >= 80.0 else 0,
        "congestion": congestion_of(idle, total, util),
    }


def iso_plus_hours(iso: str, hours: int) -> str:
    raw = (iso or "").replace("Z", "")
    if "T" not in raw:
        raw = raw.replace(" ", "T")
    try:
        dt = datetime.fromisoformat(raw)
    except ValueError:
        dt = datetime(2026, 9, 13, 21, 0, 0)
    return (dt + timedelta(hours=int(hours))).strftime("%Y-%m-%dT%H:%M:%S")


def enrich_peak_alert(station_id, name: str, horizon: int, util, created_at: str) -> dict:
    util_f = float(util or 0.0)
    reason = "%s小时后预测占用率 %.0f%%" % (horizon, util_f)
    return {
        "alert_id": "%s-h%s" % (station_id, horizon),
        "station_id": int(station_id),
        "name": name,
        "station_name": name,
        "horizon": int(horizon),
        "reason": reason,
        "message": reason,
        "alert_type": "peak",
        "severity": "high",
        "predicted_occupancy": round(util_f, 1),
        "predicted_time": iso_plus_hours(created_at, horizon),
        "created_at": created_at,
    }


def stamp_load(points: list, capacity_kw: float, warn_kw: float) -> list:
    out = []
    for x in points:
        row = dict(x)
        row["capacity_kw"] = capacity_kw
        row["warning_threshold_kw"] = warn_kw
        out.append(row)
    return out


def collect_faults(spark: SparkSession) -> list:
    rows = spark.sql(
        """
        SELECT CAST(p.id AS BIGINT) AS pile_id,
               p.code AS code,
               CAST(p.station_id AS BIGINT) AS station_id,
               s.name AS station_name,
               p.status AS status,
               CAST(p.last_online_at AS STRING) AS fault_time
        FROM ods_pile p
        JOIN ods_station s ON CAST(p.station_id AS BIGINT) = CAST(s.id AS BIGINT)
        WHERE lower(p.status) = 'fault'
        ORDER BY CAST(p.station_id AS BIGINT), CAST(p.id AS BIGINT)
        """
    ).collect()
    faults = []
    for r in rows:
        faults.append(
            {
                "pile_id": int(r["pile_id"]),
                "station_id": int(r["station_id"]),
                "station_name": r["station_name"] or ("站 %s" % r["station_id"]),
                "code": r["code"],
                "fault_type": "fault",
                "fault_code": None,
                "fault_time": r["fault_time"],
                "status": "fault",
            }
        )
    return faults


def occupancy_pct(idle, total) -> float:
    total_n = int(total or 0)
    idle_n = int(idle or 0)
    if total_n <= 0:
        return 0.0
    return round(max(0.0, (total_n - idle_n) * 100.0 / total_n), 1)


def haversine_km(lat1, lon1, lat2, lon2) -> float:
    radius = 6371.0
    p1, p2 = math.radians(float(lat1)), math.radians(float(lat2))
    dphi = math.radians(float(lat2) - float(lat1))
    dlamb = math.radians(float(lon2) - float(lon1))
    arc = (
        math.sin(dphi / 2) ** 2
        + math.cos(p1) * math.cos(p2) * math.sin(dlamb / 2) ** 2
    )
    return round(2 * radius * math.asin(min(1.0, math.sqrt(arc))), 2)


def predicted_occupancy(station: dict) -> float:
    slot = ((station.get("forecast") or {}).get("h1") or {})
    if slot.get("util") is not None:
        return round(float(slot["util"]), 1)
    return occupancy_pct(station.get("idle"), station.get("total"))


def collect_dispatch(stations: list, created_at: str, limit: int = 3) -> list:
    """高峰/高占用站 → 更空闲且更近的站。无高峰时兜底取占用最高的 3 站。"""
    usable = []
    for station in stations or []:
        try:
            float(station["latitude"])
            float(station["longitude"])
        except (KeyError, TypeError, ValueError):
            continue
        if int(station.get("total") or 0) <= 0:
            continue
        usable.append(station)
    if not usable:
        return []

    peaks = [
        s
        for s in usable
        if int(((s.get("forecast") or {}).get("h1") or {}).get("is_peak") or 0)
        or predicted_occupancy(s) >= 80.0
    ]
    sources = sorted(peaks, key=predicted_occupancy, reverse=True)
    if not sources:
        sources = sorted(
            usable, key=lambda s: occupancy_pct(s.get("idle"), s.get("total")), reverse=True
        )[:3]

    used = set()
    out = []
    for src in sources:
        sid = int(src["station_id"])
        src_idle = int(src.get("idle") or 0)
        src_pred = predicted_occupancy(src)
        best = None
        for dst in usable:
            did = int(dst["station_id"])
            if did == sid or did in used:
                continue
            idle = int(dst.get("idle") or 0)
            if idle <= src_idle:
                continue
            dst_occ = occupancy_pct(dst.get("idle"), dst.get("total"))
            km = haversine_km(
                src["latitude"], src["longitude"], dst["latitude"], dst["longitude"]
            )
            preferred = idle >= 2 and dst_occ < 40.0
            score = (0 if preferred else 1, km, dst_occ, -idle)
            if best is None or score < best[0]:
                best = (score, dst, km, dst_occ, idle)
        if best is None:
            continue
        _, dst, km, dst_occ, idle = best
        used.add(int(dst["station_id"]))
        sname = src.get("name") or ("站 %s" % sid)
        dname = dst.get("name") or ("站 %s" % dst["station_id"])
        reason = "预测占用 %.0f%%，引导至 %.1fkm 外空闲站（空闲 %d 桩）" % (
            src_pred,
            km,
            idle,
        )
        out.append(
            {
                "source_station_id": sid,
                "source_station_name": sname,
                "source_predicted_occupancy": src_pred,
                "recommended_station_id": int(dst["station_id"]),
                "recommended_station_name": dname,
                "recommended_idle_piles": idle,
                "recommended_occupancy": dst_occ,
                "distance_km": km,
                "expected_improvement": round(max(0.0, src_pred - dst_occ), 1),
                "reason": reason,
                "created_at": created_at,
                "from_name": sname,
                "to_name": dname,
                "from_id": sid,
                "to_id": int(dst["station_id"]),
            }
        )
        if len(out) >= limit:
            break
    return out


def window_targets(stop: date, prev_kwh, prev_rev) -> dict:
    return {
        "date": stop.isoformat(),
        "charge_kwh_target": None if prev_kwh is None else round(prev_kwh, 2),
        "revenue_target": None if prev_rev is None else round(prev_rev, 2),
        "availability_target": 90.0,
    }


def _window_metrics(spark: SparkSession, start: date, end: date, hourly: str):
    s, e = start.isoformat(), end.isoformat()
    rows = spark.sql(
        """
        SELECT hour_of_day AS hour,
               ROUND(SUM(kwh), 2) AS kwh_sum,
               ROUND(SUM(kwh) / COUNT(DISTINCT date_format(hour_ts, 'yyyy-MM-dd')), 2) AS kwh_avg
        FROM dws
        WHERE date_format(hour_ts, 'yyyy-MM-dd') BETWEEN '%s' AND '%s'
        GROUP BY hour_of_day
        """
        % (s, e)
    ).collect()
    key = "kwh_sum" if start == end else "kwh_avg"
    by = {int(r["hour"]): float(r[key] or 0.0) for r in rows}
    load = [{"hour": h, "kwh": round(by.get(h, 0.0), 2)} for h in range(24)]
    tot = spark.sql(
        """
        SELECT ROUND(SUM(kwh), 2) AS kwh,
               ROUND(SUM(d.kwh * CAST(s.price AS DOUBLE)), 2) AS revenue
        FROM dws d
        JOIN ods_station s ON d.station_id = CAST(s.id AS BIGINT)
        WHERE date_format(d.hour_ts, 'yyyy-MM-dd') BETWEEN '%s' AND '%s'
        """
        % (s, e)
    ).collect()[0]
    return load, float(tot["kwh"] or 0.0), float(tot["revenue"] or 0.0)


def collect_period_windows(spark: SparkSession, sim_day: date) -> dict:
    """period=1/7/30：当前窗口 KPI + 上一窗口（不够则 yesterday_* 为 null）。"""
    bounds = spark.sql(
        """
        SELECT date_format(min(hour_ts), 'yyyy-MM-dd') AS dmin,
               date_format(max(hour_ts), 'yyyy-MM-dd') AS dmax
        FROM dws
        """
    ).collect()[0]
    if not bounds["dmin"] or not bounds["dmax"]:
        return {}
    min_day = date.fromisoformat(str(bounds["dmin"]))
    max_day = date.fromisoformat(str(bounds["dmax"]))
    end = min(sim_day, max_day)

    def clip(days: int):
        start = end - timedelta(days=days - 1)
        if start < min_day:
            start = min_day
        return start, end

    def previous(start: date, stop: date):
        n = (stop - start).days + 1
        p_end = start - timedelta(days=1)
        if p_end < min_day:
            return None, None
        p_start = p_end - timedelta(days=n - 1)
        if p_start < min_day:
            p_start = min_day
        return p_start, p_end

    windows = {}
    for key, days in (("1", 1), ("7", 7), ("30", 30)):
        start, stop = clip(days)
        load, kwh, rev = _window_metrics(spark, start, stop, "sum")
        peak = max(load, key=lambda x: x["kwh"])
        p_start, p_stop = previous(start, stop)
        prev_load = prev_kwh = prev_rev = None
        if p_start and p_stop:
            prev_load, prev_kwh, prev_rev = _window_metrics(spark, p_start, p_stop, "sum")
        windows[key] = {
            "start": start.isoformat(),
            "end": stop.isoformat(),
            "prev_start": p_start.isoformat() if p_start else None,
            "prev_end": p_stop.isoformat() if p_stop else None,
            "kpis": {
                "today_kwh": round(kwh, 2),
                "today_revenue": round(rev, 2),
                "yesterday_kwh": None if prev_kwh is None else round(prev_kwh, 2),
                "yesterday_charge_kwh": None if prev_kwh is None else round(prev_kwh, 2),
                "yesterday_revenue": None if prev_rev is None else round(prev_rev, 2),
                "peak_hour": "%02d:00" % peak["hour"],
            },
            "load_today": load,
            "yesterday_load": prev_load,
            "targets": window_targets(stop, prev_kwh, prev_rev),
        }
    return windows


def quality_from_disk() -> dict:
    report_path = WORK_DIR / "qa" / "report.json"
    keys = (
        "ods_rows",
        "null_start_time",
        "negative_kwh",
        "dup_order_no",
        "orphan_station",
        "dwd_rows",
    )
    out = {}
    if report_path.is_file():
        qa = json.loads(report_path.read_text(encoding="utf-8"))
        for k in keys:
            if k in qa:
                out[k] = qa[k]
    contract = load_contract()
    if contract:
        probe = contract.get("expected_probe", {})
        derived = contract.get("derived", {})
        for k in keys:
            if k not in out and k in probe:
                out[k] = probe[k]
        if "dwd_rows" not in out:
            out["dwd_rows"] = derived.get("dwd_rows")
    return out


def collect_ops_panels(spark: SparkSession) -> tuple[dict, dict]:
    """范例大屏可补块（不改清洗、不重训）：全窗口 KPI、小时均负荷、工作日/周末、分城、快慢桩。"""
    kpi_extra = {
        "order_count": 0,
        "total_kwh": 0.0,
        "total_revenue": 0.0,
        "active_stations": 0,
    }
    panels = {
        "load_hour_avg": [{"hour": h, "kwh": 0.0} for h in range(24)],
        "weekday_weekend": {"weekday_kwh": 0.0, "weekend_kwh": 0.0},
        "regions": [],
        "pile_types": [],
    }

    hod_rows = spark.sql(
        """
        SELECT hour_of_day AS hour,
               ROUND(
                 SUM(kwh) / COUNT(DISTINCT date_format(hour_ts, 'yyyy-MM-dd')),
                 2
               ) AS kwh
        FROM dws
        GROUP BY hour_of_day
        """
    ).collect()
    by_hod = {int(r["hour"]): float(r["kwh"] or 0.0) for r in hod_rows}
    panels["load_hour_avg"] = [
        {"hour": h, "kwh": round(by_hod.get(h, 0.0), 2)} for h in range(24)
    ]

    ww = {
        int(r["is_weekend"] or 0): float(r["kwh"] or 0.0)
        for r in spark.sql(
            """
            SELECT CAST(is_weekend AS INT) AS is_weekend,
                   ROUND(SUM(kwh), 2) AS kwh
            FROM dws
            GROUP BY CAST(is_weekend AS INT)
            """
        ).collect()
    }
    panels["weekday_weekend"] = {
        "weekday_kwh": round(ww.get(0, 0.0), 2),
        "weekend_kwh": round(ww.get(1, 0.0), 2),
        "note": "DWS 分摊电量；weekday=周一至周五，weekend=周六日",
    }

    pile_rows = spark.sql(
        """
        SELECT
          CASE
            WHEN lower(type) = 'fast' THEN '直流'
            WHEN lower(type) = 'slow' THEN '交流'
            ELSE type
          END AS type,
          SUM(CASE WHEN status = 'idle' THEN 1 ELSE 0 END) AS idle,
          SUM(CASE WHEN status = 'busy' THEN 1 ELSE 0 END) AS busy,
          SUM(CASE WHEN status = 'fault' THEN 1 ELSE 0 END) AS fault
        FROM ods_pile
        GROUP BY
          CASE
            WHEN lower(type) = 'fast' THEN '直流'
            WHEN lower(type) = 'slow' THEN '交流'
            ELSE type
          END
        """
    ).collect()
    types_out = []
    for r in pile_rows:
        idle = int(r["idle"] or 0)
        busy = int(r["busy"] or 0)
        fault = int(r["fault"] or 0)
        denom = idle + busy
        util = 0.0 if denom <= 0 else round(100.0 * busy / denom, 1)
        types_out.append(
            {
                "type": r["type"] or "其他",
                "idle": idle,
                "busy": busy,
                "fault": fault,
                "util": util,
            }
        )
    order = {"直流": 0, "交流": 1}
    types_out.sort(key=lambda x: order.get(x["type"], 9))
    panels["pile_types"] = types_out

    src = dwd_path()
    if not src.is_file():
        print("ADS 运营块：无 DWD，跳过全窗口 KPI 与分城", src)
        return kpi_extra, panels

    dwd = (
        read_csv(spark, src)
        .withColumn("station_id", F.col("station_id").cast("long"))
        .withColumn("kwh", F.col("kwh").cast("double"))
        .withColumn("amount", F.col("amount").cast("double"))
    )
    dwd.createOrReplaceTempView("dwd_ops")
    tot = spark.sql(
        """
        SELECT COUNT(*) AS order_count,
               ROUND(SUM(kwh), 2) AS total_kwh,
               ROUND(SUM(amount), 2) AS total_revenue,
               COUNT(DISTINCT station_id) AS active_stations
        FROM dwd_ops
        """
    ).collect()[0]
    kpi_extra = {
        "order_count": int(tot["order_count"] or 0),
        "total_kwh": float(tot["total_kwh"] or 0.0),
        "total_revenue": float(tot["total_revenue"] or 0.0),
        "active_stations": int(tot["active_stations"] or 0),
    }
    region_rows = spark.sql(
        """
        SELECT substring(s.address, 1, 3) AS city,
               ROUND(SUM(o.kwh), 2) AS kwh,
               ROUND(SUM(o.amount), 2) AS amount
        FROM dwd_ops o
        JOIN ods_station s ON o.station_id = CAST(s.id AS BIGINT)
        GROUP BY substring(s.address, 1, 3)
        ORDER BY amount DESC
        """
    ).collect()
    regions = []
    for r in region_rows:
        kwh = float(r["kwh"] or 0.0)
        amount = float(r["amount"] or 0.0)
        regions.append(
            {
                "city": r["city"] or "未知",
                "kwh": round(kwh, 2),
                "amount": round(amount, 2),
                "yuan_per_kwh": round(amount / kwh, 2) if kwh > 0 else 0.0,
            }
        )
    panels["regions"] = regions
    return kpi_extra, panels


def stage_ads(spark: SparkSession) -> int:
    """NO.116：SparkSQL 从 DWS 出大屏 JSON + MLlib 训练宽表。

    1h/6h/24h 用「最新时刻 / 近 6h 均值 / 昨日同时段」占位，
    真正的 MLlib 预测留给 --stage train。
    另写范例大屏运营块：全窗口 KPI、load_hour_avg、weekday_weekend、regions、pile_types。
    """
    src = dws_path()
    if not src.is_file():
        print("未找到 DWS，先跑 dws:", src)
        rc = stage_dws(spark)
        if rc != 0:
            return rc
    if not src.is_file():
        raise FileNotFoundError(src)

    sim = "2026-09-13"
    sim_now = "2026-09-13T21:00:00"
    contract = load_contract()
    if contract and "params" in contract:
        sim = contract["params"].get("end_date", sim)
        sim_now = contract["params"].get("sim_now", sim_now).replace(" ", "T")

    dws = (
        spark.read.option("header", True)
        .option("encoding", "UTF-8")
        .csv(file_uri(src))
        .withColumn("station_id", F.col("station_id").cast("long"))
        .withColumn("hour_ts", F.to_timestamp("hour_ts"))
        .withColumn("kwh", F.col("kwh").cast("double"))
        .withColumn("order_count", F.col("order_count").cast("int"))
        .withColumn("busy_piles", F.col("busy_piles").cast("int"))
        .withColumn("idle_piles", F.col("idle_piles").cast("int"))
        .withColumn("total_piles", F.col("total_piles").cast("int"))
        .withColumn("weekday", F.col("weekday").cast("int"))
        .withColumn("is_weekend", F.col("is_weekend").cast("int"))
        .withColumn("is_holiday", F.col("is_holiday").cast("int"))
        .withColumn("hour_of_day", F.col("hour_of_day").cast("int"))
    )
    dws.createOrReplaceTempView("dws")

    kind, _orders, stations, piles = load_ods(spark)
    stations.createOrReplaceTempView("ods_station")
    piles.createOrReplaceTempView("ods_pile")
    print("ADS 用 DWS + ODS source:", kind)
    kpi_extra, ops_panels = collect_ops_panels(spark)

    w = Window.partitionBy("station_id").orderBy("hour_ts")
    w24 = Window.partitionBy("station_id").orderBy("hour_ts").rowsBetween(-24, -1)
    feat = (
        dws.withColumn("lag1_kwh", F.lag("kwh", 1).over(w))
        .withColumn("mean_24h_kwh", F.avg("kwh").over(w24))
        .withColumn("label_kwh_1", F.lead("kwh", 1).over(w))
        .withColumn("label_idle_1", F.lead("idle_piles", 1).over(w))
        .withColumn("label_kwh_6", F.lead("kwh", 6).over(w))
        .withColumn("label_idle_6", F.lead("idle_piles", 6).over(w))
        .withColumn("label_kwh_24", F.lead("kwh", 24).over(w))
        .withColumn("label_idle_24", F.lead("idle_piles", 24).over(w))
    )
    feat.createOrReplaceTempView("dws_feat")
    wide = spark.sql(
        """
        SELECT station_id, hour_ts, kwh, idle_piles, busy_piles, total_piles,
               hour_of_day, weekday, is_weekend, is_holiday,
               lag1_kwh, mean_24h_kwh, horizon, label_kwh, label_idle
        FROM (
          SELECT station_id, hour_ts, kwh, idle_piles, busy_piles, total_piles,
                 hour_of_day, weekday, is_weekend, is_holiday,
                 lag1_kwh, mean_24h_kwh,
                 1 AS horizon, label_kwh_1 AS label_kwh, label_idle_1 AS label_idle
          FROM dws_feat
          UNION ALL
          SELECT station_id, hour_ts, kwh, idle_piles, busy_piles, total_piles,
                 hour_of_day, weekday, is_weekend, is_holiday,
                 lag1_kwh, mean_24h_kwh,
                 6, label_kwh_6, label_idle_6
          FROM dws_feat
          UNION ALL
          SELECT station_id, hour_ts, kwh, idle_piles, busy_piles, total_piles,
                 hour_of_day, weekday, is_weekend, is_holiday,
                 lag1_kwh, mean_24h_kwh,
                 24, label_kwh_24, label_idle_24
          FROM dws_feat
        )
        WHERE label_kwh IS NOT NULL
        """
    )
    train_dest = WORK_DIR / "ads" / "train"
    write_csv(wide, train_dest)
    train_n = wide.count()
    print("ADS train wide rows:", train_n, " file:", train_dest / "train.csv")

    today_load = spark.sql(
        """
        SELECT hour_of_day AS hour, ROUND(SUM(kwh), 2) AS kwh
        FROM dws
        WHERE date_format(hour_ts, 'yyyy-MM-dd') = '%s'
        GROUP BY hour_of_day
        ORDER BY hour
        """
        % sim
    ).collect()
    by_hour = {int(r["hour"]): float(r["kwh"]) for r in today_load}
    load_today = [{"hour": h, "kwh": round(by_hour.get(h, 0.0), 2)} for h in range(24)]
    today_kwh = round(sum(x["kwh"] for x in load_today), 2)
    peak = max(load_today, key=lambda x: x["kwh"])
    peak_hour = "%02d:00" % peak["hour"]

    price_rev = spark.sql(
        """
        SELECT ROUND(SUM(d.kwh * CAST(s.price AS DOUBLE)), 2) AS revenue
        FROM dws d
        JOIN ods_station s ON d.station_id = CAST(s.id AS BIGINT)
        WHERE date_format(d.hour_ts, 'yyyy-MM-dd') = '%s'
        """
        % sim
    ).collect()[0]["revenue"]
    today_revenue = float(price_rev or 0.0)

    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW station_dim AS
        SELECT CAST(s.id AS BIGINT) AS station_id,
               s.name AS name,
               CAST(s.longitude AS DOUBLE) AS longitude,
               CAST(s.latitude AS DOUBLE) AS latitude,
               COUNT(p.id) AS total_piles,
               SUM(CASE WHEN p.status = 'idle' THEN 1 ELSE 0 END) AS snap_idle,
               SUM(CASE WHEN p.status = 'busy' THEN 1 ELSE 0 END) AS snap_busy,
               SUM(CASE WHEN p.status = 'fault' THEN 1 ELSE 0 END) AS snap_fault,
               ROUND(SUM(CAST(p.power_kw AS DOUBLE)), 1) AS capacity_kw
        FROM ods_station s
        LEFT JOIN ods_pile p ON CAST(s.id AS BIGINT) = CAST(p.station_id AS BIGINT)
        GROUP BY CAST(s.id AS BIGINT), s.name, s.longitude, s.latitude
        """
    )
    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW dws_latest AS
        SELECT d.*
        FROM dws d
        JOIN (
          SELECT station_id, max(hour_ts) AS hour_ts
          FROM dws
          GROUP BY station_id
        ) m ON d.station_id = m.station_id AND d.hour_ts = m.hour_ts
        """
    )
    # 桩状态用 ODS 快照：DWS 最新小时只有当小时有单的站，会漏掉全空闲站
    snap = spark.sql(
        """
        SELECT SUM(snap_idle) AS idle_piles,
               SUM(snap_busy) AS busy_piles,
               SUM(snap_fault) AS fault_piles
        FROM station_dim
        """
    ).collect()[0]
    busy_piles = int(snap["busy_piles"] or 0)
    idle_piles = int(snap["idle_piles"] or 0)
    fault_piles = int(snap["fault_piles"] or 0)

    hod_avg = {
        int(r["hour_of_day"]): float(r["avg_kwh"])
        for r in spark.sql(
            """
            SELECT hour_of_day,
                   SUM(kwh) / COUNT(DISTINCT date_format(hour_ts, 'yyyy-MM-dd')) AS avg_kwh
            FROM dws
            GROUP BY hour_of_day
            """
        ).collect()
    }
    latest_hod = spark.sql("SELECT hour(max(hour_ts)) AS h FROM dws").collect()[0]["h"]
    latest_hod = int(latest_hod if latest_hod is not None else 20)
    load_forecast_24h = []
    for offset in range(1, 25):
        hod = (latest_hod + offset) % 24
        load_forecast_24h.append({"offset": offset, "kwh": round(hod_avg.get(hod, 0.0), 2)})

    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW hist6 AS
        SELECT d.station_id,
               AVG(d.kwh) AS kwh6,
               AVG(d.idle_piles) AS idle6,
               AVG(d.total_piles) AS total6
        FROM dws d
        JOIN (SELECT station_id, max(hour_ts) AS t FROM dws GROUP BY station_id) m
          ON d.station_id = m.station_id
        WHERE d.hour_ts > m.t - INTERVAL 6 HOURS
        GROUP BY d.station_id
        """
    )
    spark.sql(
        """
        CREATE OR REPLACE TEMP VIEW yday AS
        SELECT d.station_id, d.kwh, d.idle_piles, d.total_piles
        FROM dws d
        JOIN (SELECT station_id, max(hour_ts) AS t FROM dws GROUP BY station_id) m
          ON d.station_id = m.station_id
         AND d.hour_ts = m.t - INTERVAL 24 HOURS
        """
    )
    station_rows = spark.sql(
        """
        SELECT
          d.station_id, d.name, d.longitude, d.latitude,
          d.total_piles, d.snap_idle, d.capacity_kw,
          l.kwh AS kwh1, l.idle_piles AS dws_idle1, l.total_piles AS dws_total1,
          h.kwh6, h.idle6, h.total6,
          y.kwh AS kwh24, y.idle_piles AS idle24, y.total_piles AS total24
        FROM station_dim d
        LEFT JOIN dws_latest l ON d.station_id = l.station_id
        LEFT JOIN hist6 h ON d.station_id = h.station_id
        LEFT JOIN yday y ON d.station_id = y.station_id
        ORDER BY d.station_id
        """
    ).collect()

    stations_out = []
    alerts = []
    for r in station_rows:
        sid = int(r["station_id"])
        name = r["name"] or ("站 %s" % sid)
        total = int(r["total_piles"] or 0)
        idle = int(r["snap_idle"] or 0)
        dws_total = int(r["dws_total1"] if r["dws_total1"] is not None else total)
        dws_idle = int(r["dws_idle1"] if r["dws_idle1"] is not None else idle)
        cap = float(r["capacity_kw"] or 0.0)
        warn = round(cap * 0.8, 1)
        h1 = forecast_slot(r["kwh1"] or 0.0, dws_idle, dws_total)
        h6 = forecast_slot(
            r["kwh6"] if r["kwh6"] is not None else (r["kwh1"] or 0.0),
            int(r["idle6"] if r["idle6"] is not None else dws_idle),
            int(r["total6"] if r["total6"] is not None else dws_total),
        )
        h24 = forecast_slot(
            r["kwh24"] if r["kwh24"] is not None else (r["kwh1"] or 0.0),
            int(r["idle24"] if r["idle24"] is not None else dws_idle),
            int(r["total24"] if r["total24"] is not None else dws_total),
        )
        stations_out.append(
            {
                "station_id": sid,
                "name": name,
                "idle": idle,
                "total": total,
                "latitude": None if r["latitude"] is None else float(r["latitude"]),
                "longitude": None if r["longitude"] is None else float(r["longitude"]),
                "capacity_kw": cap,
                "warning_threshold_kw": warn,
                "forecast": {"h1": h1, "h6": h6, "h24": h24},
            }
        )
        for horizon, slot in ((1, h1), (6, h6), (24, h24)):
            if slot["is_peak"]:
                alerts.append(
                    enrich_peak_alert(sid, name, horizon, slot["util"], sim_now)
                )

    faults = collect_faults(spark)
    sim_day = date.fromisoformat(sim[:10])
    windows = collect_period_windows(spark, sim_day)
    cap_row = spark.sql("SELECT ROUND(SUM(capacity_kw), 1) AS cap FROM station_dim").collect()[0]
    global_cap = float(cap_row["cap"] or 0.0)
    global_warn = round(global_cap * 0.8, 1)
    load_today = stamp_load(load_today, global_cap, global_warn)
    for win in windows.values():
        win["load_today"] = stamp_load(win.get("load_today") or [], global_cap, global_warn)
        if win.get("yesterday_load") is not None:
            win["yesterday_load"] = stamp_load(win["yesterday_load"], global_cap, global_warn)
    day_win = windows.get("1") or {}
    if day_win:
        day_win["load_today"] = load_today
        k = day_win.setdefault("kpis", {})
        k["today_kwh"] = today_kwh
        k["today_revenue"] = round(today_revenue, 2)
        k["peak_hour"] = peak_hour
    day_kpis = day_win.get("kpis") or {}
    dispatch = collect_dispatch(stations_out, sim_now)

    dashboard = {
        "generated_at": sim_now,
        "latest_data_time": sim_now,
        "data_source": "ads",
        "freshness_status": "ok",
        "kpis": {
            "today_kwh": today_kwh,
            "today_revenue": round(today_revenue, 2),
            "yesterday_kwh": day_kpis.get("yesterday_kwh"),
            "yesterday_charge_kwh": day_kpis.get("yesterday_charge_kwh"),
            "yesterday_revenue": day_kpis.get("yesterday_revenue"),
            "idle_piles": idle_piles,
            "busy_piles": busy_piles,
            "fault_piles": int(fault_piles),
            "peak_hour": peak_hour,
            "alert_count": len(alerts),
            "order_count": kpi_extra["order_count"],
            "total_kwh": kpi_extra["total_kwh"],
            "total_revenue": kpi_extra["total_revenue"],
            "active_stations": kpi_extra["active_stations"],
        },
        "targets": day_win.get("targets") or window_targets(sim_day, day_kpis.get("yesterday_kwh"), day_kpis.get("yesterday_revenue")),
        "quality": quality_from_disk(),
        "load_today": load_today,
        "yesterday_load": day_win.get("yesterday_load"),
        "windows": windows,
        "faults": faults,
        "dispatch": dispatch,
        "load_forecast_24h": load_forecast_24h,
        "load_hour_avg": ops_panels["load_hour_avg"],
        "weekday_weekend": ops_panels["weekday_weekend"],
        "regions": ops_panels["regions"],
        "pile_types": ops_panels["pile_types"],
        "stations": stations_out,
        "alerts": alerts,
        "note": "ADS 占位预测来自 DWS 最新/近6h/昨日同时段，MLlib 在 --stage train 替换",
    }
    kpi_dir = WORK_DIR / "ads" / "kpis"
    kpi_dir.mkdir(parents=True, exist_ok=True)
    dash_path = kpi_dir / "dashboard.json"
    dash_path.write_text(
        json.dumps(dashboard, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print("ADS dashboard ->", dash_path)
    print(
        json.dumps(dashboard["kpis"], ensure_ascii=False),
        "stations",
        len(stations_out),
        "alerts",
        len(alerts),
        "faults",
        len(faults),
        "dispatch",
        len(dispatch),
        "windows",
        sorted(windows.keys()),
    )
    try_hdfs_put(dash_path, "/user/charging/ads/kpis")
    try_hdfs_put(train_dest / "train.csv", "/user/charging/ads/train")
    print("大屏接真数：DASHBOARD_DATA_FILE=%s python3 dashboard/app.py" % dash_path)
    return 0


FEATURE_COLS = [
    "hour_of_day",
    "weekday",
    "is_weekend",
    "is_holiday",
    "lag1_kwh",
    "mean_24h_kwh",
    "horizon",
    "kwh",
    "idle_piles",
]


def train_csv_path() -> Path:
    return WORK_DIR / "ads" / "train" / "train.csv"


def dash_path() -> Path:
    return WORK_DIR / "ads" / "kpis" / "dashboard.json"


def sim_now_pair() -> tuple[str, str]:
    """返回 (JSON 用 2026-09-13T21:00:00, MySQL 用 2026-09-13 21:00:00)。"""
    iso = "2026-09-13T21:00:00"
    contract = load_contract()
    if contract and "params" in contract:
        iso = contract["params"].get("sim_now", iso.replace("T", " ")).replace(" ", "T")
        if "T" not in iso:
            iso = iso.replace(" ", "T")
    return iso, iso.replace("T", " ")


def load_dws_typed(spark: SparkSession):
    src = dws_path()
    if not src.is_file():
        raise FileNotFoundError(src)
    return (
        spark.read.option("header", True)
        .option("encoding", "UTF-8")
        .csv(file_uri(src))
        .withColumn("station_id", F.col("station_id").cast("long"))
        .withColumn("hour_ts", F.to_timestamp("hour_ts"))
        .withColumn("kwh", F.col("kwh").cast("double"))
        .withColumn("idle_piles", F.col("idle_piles").cast("int"))
        .withColumn("total_piles", F.col("total_piles").cast("int"))
        .withColumn("weekday", F.col("weekday").cast("int"))
        .withColumn("is_weekend", F.col("is_weekend").cast("int"))
        .withColumn("is_holiday", F.col("is_holiday").cast("int"))
        .withColumn("hour_of_day", F.col("hour_of_day").cast("int"))
    )


def latest_station_features(spark: SparkSession):
    """每站最新小时 + lag-1 / 近 24h 均值，给 MLlib 推理用。"""
    dws = load_dws_typed(spark)
    w = Window.partitionBy("station_id").orderBy("hour_ts")
    w24 = Window.partitionBy("station_id").orderBy("hour_ts").rowsBetween(-24, -1)
    feat = (
        dws.withColumn("lag1_kwh", F.lag("kwh", 1).over(w))
        .withColumn("mean_24h_kwh", F.avg("kwh").over(w24))
    )
    latest = feat.join(
        feat.groupBy("station_id").agg(F.max("hour_ts").alias("hour_ts")),
        ["station_id", "hour_ts"],
    ).withColumn(
        "lag1_kwh", F.coalesce(F.col("lag1_kwh"), F.col("kwh"), F.lit(0.0))
    ).withColumn(
        "mean_24h_kwh", F.coalesce(F.col("mean_24h_kwh"), F.col("kwh"), F.lit(0.0))
    )
    return latest.select(
        "station_id",
        "hour_of_day",
        "weekday",
        "is_weekend",
        "is_holiday",
        "lag1_kwh",
        "mean_24h_kwh",
        "idle_piles",
        "total_piles",
        "kwh",
    )


def clip_forecast(df):
    idle = F.least(
        F.col("total_piles"),
        F.greatest(F.lit(0), F.round(F.col("pred_idle_raw")).cast("int")),
    )
    kwh = F.greatest(F.lit(0.0), F.col("pred_kwh_raw"))
    util = F.when(
        F.col("total_piles") <= 0, F.lit(0.0)
    ).otherwise(
        (F.col("total_piles") - idle) * 100.0 / F.col("total_piles")
    )
    util = F.least(F.lit(100.0), F.greatest(F.lit(0.0), util))
    return (
        df.withColumn("pred_kwh", F.round(kwh, 2))
        .withColumn("pred_idle", idle)
        .withColumn("pred_util", F.round(util, 1))
        .withColumn("is_peak", F.when(util >= 80.0, F.lit(1)).otherwise(F.lit(0)))
    )


def write_mysql_forecast(rows: list[dict], generated_at: str) -> int:
    """只写 load_forecast。禁止碰 charge_order / ODS。"""
    try:
        import common as bd_common
    except ImportError:
        print("写库跳过：找不到 bigdata/common.py")
        return 0
    try:
        conn = bd_common.connect()
    except Exception as exc:
        print("写库跳过：MySQL 连不上:", exc)
        return 0
    sql = (
        "INSERT INTO load_forecast ("
        "station_id, generated_at, horizon_hours, pred_kwh, pred_idle, "
        "pred_util, is_peak, congestion"
        ") VALUES (%s,%s,%s,%s,%s,%s,%s,%s) "
        "ON DUPLICATE KEY UPDATE pred_kwh=VALUES(pred_kwh), "
        "pred_idle=VALUES(pred_idle), pred_util=VALUES(pred_util), "
        "is_peak=VALUES(is_peak), congestion=VALUES(congestion)"
    )
    payload = [
        (
            int(r["station_id"]),
            generated_at,
            int(r["horizon"]),
            float(r["pred_kwh"]),
            int(r["pred_idle"]),
            float(r["pred_util"]),
            int(r["is_peak"]),
            r["congestion"],
        )
        for r in rows
    ]
    try:
        with conn.cursor() as cur:
            cur.execute("SHOW TABLES LIKE 'load_forecast'")
            if cur.fetchone() is None:
                print("写库跳过：没有 load_forecast 表，先跑 sql/patch_v4_load_forecast.sql")
                return 0
            cur.executemany(sql, payload)
        conn.commit()
        print("MySQL load_forecast 写入", len(payload), "行  generated_at=", generated_at)
        return len(payload)
    except Exception as exc:
        conn.rollback()
        print("写库失败（JSON 已落地）:", exc)
        return 0
    finally:
        conn.close()


def stage_train(spark: SparkSession) -> int:
    """NO.117：MLlib 随机森林预测 1h/6h/24h，写 ADS JSON + MySQL load_forecast。"""
    from pyspark.ml.feature import VectorAssembler
    from pyspark.ml.regression import RandomForestRegressor

    src = train_csv_path()
    if not src.is_file() or not dash_path().is_file():
        print("未找到 ADS，先跑 ads:", src)
        rc = stage_ads(spark)
        if rc != 0:
            return rc
    if not src.is_file():
        raise FileNotFoundError(src)

    wide = (
        spark.read.option("header", True)
        .option("encoding", "UTF-8")
        .csv(file_uri(src))
        .withColumn("hour_of_day", F.col("hour_of_day").cast("int"))
        .withColumn("weekday", F.col("weekday").cast("int"))
        .withColumn("is_weekend", F.col("is_weekend").cast("int"))
        .withColumn("is_holiday", F.col("is_holiday").cast("int"))
        .withColumn("lag1_kwh", F.col("lag1_kwh").cast("double"))
        .withColumn("mean_24h_kwh", F.col("mean_24h_kwh").cast("double"))
        .withColumn("horizon", F.col("horizon").cast("int"))
        .withColumn("kwh", F.col("kwh").cast("double"))
        .withColumn("idle_piles", F.col("idle_piles").cast("int"))
        .withColumn("label_kwh", F.col("label_kwh").cast("double"))
        .withColumn("label_idle", F.col("label_idle").cast("double"))
        .na.drop(subset=FEATURE_COLS + ["label_kwh", "label_idle"])
    )
    assembler = VectorAssembler(inputCols=FEATURE_COLS, outputCol="features")
    feat = assembler.transform(wide)
    n_train = feat.count()
    print("MLlib 训练行数:", n_train)
    if n_train < 100:
        raise RuntimeError("训练宽表太少，先检查 --stage ads")

    rf_kwh = RandomForestRegressor(
        featuresCol="features",
        labelCol="label_kwh",
        predictionCol="pred_kwh_raw",
        numTrees=16,
        maxDepth=6,
        seed=20260913,
    )
    rf_idle = RandomForestRegressor(
        featuresCol="features",
        labelCol="label_idle",
        predictionCol="pred_idle_raw",
        numTrees=16,
        maxDepth=6,
        seed=20260913,
    )
    print("拟合 kwh 模型…")
    m_kwh = rf_kwh.fit(feat)
    print("拟合 idle 模型…")
    m_idle = rf_idle.fit(feat)
    print(
        "featureImportances kwh=",
        m_kwh.featureImportances,
        " idle=",
        m_idle.featureImportances,
    )

    latest = latest_station_features(spark)
    horizons = spark.createDataFrame([(1,), (6,), (24,)], ["horizon"])
    infer_h = assembler.transform(latest.crossJoin(horizons))
    pred_h = clip_forecast(m_idle.transform(m_kwh.transform(infer_h)))

    orig_hod = F.col("hour_of_day")
    infer_24 = (
        latest.withColumn("orig_hod", orig_hod)
        .crossJoin(spark.range(1, 25).toDF("offset"))
        .withColumn(
            "horizon",
            F.when(F.col("offset") <= 2, F.lit(1))
            .when(F.col("offset") <= 12, F.lit(6))
            .otherwise(F.lit(24)),
        )
        .withColumn("hour_of_day", (F.col("orig_hod") + F.col("offset")) % 24)
        .withColumn(
            "crossed",
            F.when(F.col("orig_hod") + F.col("offset") >= 24, F.lit(1)).otherwise(F.lit(0)),
        )
        .withColumn("weekday", (F.col("weekday") + F.col("crossed")) % 7)
        .withColumn(
            "is_weekend", F.when(F.col("weekday") >= 5, F.lit(1)).otherwise(F.lit(0))
        )
        .drop("orig_hod", "crossed")
    )
    pred_24 = clip_forecast(
        m_idle.transform(m_kwh.transform(assembler.transform(infer_24)))
    )

    station_rows = pred_h.select(
        "station_id", "horizon", "pred_kwh", "pred_idle", "pred_util",
        "is_peak", "total_piles",
    ).collect()
    curve = (
        pred_24.groupBy("offset")
        .agg(F.round(F.sum("pred_kwh"), 2).alias("kwh"))
        .orderBy("offset")
        .collect()
    )
    load_forecast_24h = [
        {"offset": int(r["offset"]), "kwh": float(r["kwh"] or 0.0)} for r in curve
    ]

    by_station: dict[int, dict[int, dict]] = {}
    mysql_rows = []
    for r in station_rows:
        sid = int(r["station_id"])
        hz = int(r["horizon"])
        total = int(r["total_piles"] or 0)
        idle = int(r["pred_idle"] or 0)
        util = float(r["pred_util"] or 0.0)
        slot = {
            "kwh": float(r["pred_kwh"] or 0.0),
            "idle": idle,
            "util": util,
            "is_peak": int(r["is_peak"] or 0),
            "congestion": congestion_of(idle, total, util),
        }
        by_station.setdefault(sid, {})[hz] = slot
        mysql_rows.append(
            {
                "station_id": sid,
                "horizon": hz,
                "pred_kwh": slot["kwh"],
                "pred_idle": idle,
                "pred_util": util,
                "is_peak": slot["is_peak"],
                "congestion": slot["congestion"],
            }
        )

    payload = json.loads(dash_path().read_text(encoding="utf-8"))
    iso, mysql_ts = sim_now_pair()
    alerts = []
    for st in payload.get("stations") or []:
        sid = int(st["station_id"])
        total = int(st.get("total") or 0)
        slots = by_station.get(sid, {})
        forecast = st.get("forecast") or {}
        for key, hz in (("h1", 1), ("h6", 6), ("h24", 24)):
            if hz in slots:
                forecast[key] = slots[hz]
        st["forecast"] = forecast
        name = st.get("name") or ("站 %s" % sid)
        for hz, key in ((1, "h1"), (6, "h6"), (24, "h24")):
            slot = forecast.get(key) or {}
            if slot.get("is_peak"):
                alerts.append(
                    enrich_peak_alert(sid, name, hz, slot.get("util"), iso)
                )

    payload["generated_at"] = iso
    payload["latest_data_time"] = iso
    payload["data_source"] = payload.get("data_source") or "ads"
    payload["freshness_status"] = "ok"
    payload["load_forecast_24h"] = load_forecast_24h
    payload["alerts"] = alerts
    payload["dispatch"] = collect_dispatch(payload.get("stations") or [], iso)
    kpis = payload.setdefault("kpis", {})
    kpis["alert_count"] = len(alerts)
    payload["note"] = "MLlib RandomForestRegressor 预测 1h/6h/24h（seed=20260913）"
    dash_path().write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print("ADS dashboard 已换成 MLlib 预测 ->", dash_path())
    print(
        "stations",
        len(payload.get("stations") or []),
        "alerts",
        len(alerts),
        "kpis",
        json.dumps(kpis, ensure_ascii=False),
        "dispatch",
        len(payload.get("dispatch") or []),
    )

    forecast_df = pred_h.select(
        "station_id",
        F.col("horizon").alias("horizon_hours"),
        "pred_kwh",
        "pred_idle",
        "pred_util",
        "is_peak",
    )
    dest = WORK_DIR / "ads" / "forecast"
    write_csv(forecast_df, dest)
    try_hdfs_put(dash_path(), "/user/charging/ads/kpis")
    try_hdfs_put(dest / "forecast.csv", "/user/charging/ads/forecast")
    write_mysql_forecast(mysql_rows, mysql_ts)
    print("大屏接真数：DASHBOARD_DATA_FILE=%s python3 dashboard/app.py" % dash_path())
    return 0


def not_ready(stage: str) -> int:
    print("stage=%s 尚未实现。" % stage)
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
        if args.stage == "ads":
            return stage_ads(spark)
        if args.stage == "train":
            return stage_train(spark)
        return not_ready(args.stage)
    finally:
        spark.stop()


if __name__ == "__main__":
    raise SystemExit(main())
