#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""充电大数据流水线骨架（NO.113–118）。

迷你 ODS 在 bigdata/ods/（含脏数据）。今晚可跑：
  source ~/.hadoop_env.sh
  cd /home/zhaizihan/projects/charging-system
  spark-submit bigdata/pipeline.py --stage qa
  spark-submit bigdata/pipeline.py --stage clean

后续 stage：dws / ads / train（尚未实现，会打印说明后退出 0）。
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

os.environ.setdefault("PYSPARK_PYTHON", sys.executable)
os.environ.setdefault("PYSPARK_DRIVER_PYTHON", sys.executable)

from pyspark.sql import SparkSession, functions as F
from pyspark.sql.window import Window

ROOT = Path(__file__).resolve().parent.parent
ODS_DIR = ROOT / "bigdata" / "ods"
WORK_DIR = ROOT / "bigdata" / "work"


def spark_session(app: str) -> SparkSession:
    return (
        SparkSession.builder.appName(app)
        .master("local[*]")
        .config("spark.sql.session.timeZone", "Asia/Shanghai")
        .config("spark.ui.port", "4040")
        .getOrCreate()
    )


def file_uri(path: Path) -> str:
    return path.resolve().as_uri()


def read_csv(spark: SparkSession, path: Path):
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
    # 单文件便于查看：先写临时目录再合并
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
    orders = read_csv(spark, ODS_DIR / "charge_order.csv")
    stations = read_csv(spark, ODS_DIR / "station.csv").select("id").withColumnRenamed(
        "id", "station_pk"
    )
    piles = read_csv(spark, ODS_DIR / "pile.csv").select("id").withColumnRenamed("id", "pile_pk")
    return orders, stations, piles


def kwh_num(df):
    return df.withColumn("_kwh", F.col("kwh").cast("double"))


def duration_num(df):
    return df.withColumn("_dur", F.col("duration_seconds").cast("int"))


def stage_qa(spark: SparkSession) -> dict:
    orders, stations, piles = load_ods(spark)
    orders = kwh_num(duration_num(orders))
    ods_rows = orders.count()

    null_start = orders.filter(
        F.col("start_time").isNull() | (F.trim(F.col("start_time")) == "")
    ).count()
    negative_kwh = orders.filter(F.col("_kwh") < 0).count()
    end_before_start = orders.filter(
        F.col("start_time").isNotNull()
        & F.col("end_time").isNotNull()
        & (F.col("end_time") < F.col("start_time"))
    ).count()
    dup_order_no = (
        orders.groupBy("order_no").count().filter(F.col("count") > 1).count()
    )
    orphan_station = orders.join(
        stations, orders["station_id"] == stations["station_pk"], "left_anti"
    ).count()
    orphan_pile = orders.join(
        piles, orders["pile_id"] == piles["pile_pk"], "left_anti"
    ).count()
    status_contradiction = orders.filter(
        (F.col("status") == "settled")
        & (F.col("_dur") == 0)
        & (F.col("_kwh") > 0)
    ).count()

    report = {
        "generated_at": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
        "ods_rows": ods_rows,
        "null_start_time": null_start,
        "negative_kwh": negative_kwh,
        "end_before_start": end_before_start,
        "dup_order_no": dup_order_no,
        "orphan_station": orphan_station,
        "orphan_pile": orphan_pile,
        "status_contradiction": status_contradiction,
        "note": "迷你 ODS 自测；邓雅心足量数据到位后改读 HDFS /user/charging/ods/",
    }
    qa_dir = WORK_DIR / "qa"
    qa_dir.mkdir(parents=True, exist_ok=True)
    report_path = qa_dir / "report.json"
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print("QA report ->", report_path)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return report


def stage_clean(spark: SparkSession) -> None:
    orders, stations, piles = load_ods(spark)
    orders = kwh_num(duration_num(orders))
    before = orders.count()

    dwd = orders.filter(
        F.col("start_time").isNotNull() & (F.trim(F.col("start_time")) != "")
    )
    dwd = dwd.filter((F.col("_kwh").isNull()) | (F.col("_kwh") >= 0))
    dwd = dwd.filter(
        F.col("end_time").isNull() | (F.col("end_time") >= F.col("start_time"))
    )
    dwd = dwd.join(stations, dwd["station_id"] == stations["station_pk"], "inner")
    dwd = dwd.join(piles, dwd["pile_id"] == piles["pile_pk"], "inner")
    dwd = dwd.filter(
        ~(
            (F.col("status") == "settled")
            & (F.col("_dur") == 0)
            & (F.col("_kwh") > 0)
        )
    )
    w = Window.partitionBy("order_no").orderBy(
        F.col("updated_at").desc(), F.col("id").desc()
    )
    dwd = (
        dwd.withColumn("_rn", F.row_number().over(w))
        .filter(F.col("_rn") == 1)
        .drop("_rn", "_kwh", "_dur", "station_pk", "pile_pk", "dq_tag")
    )
    after = dwd.count()
    dest = WORK_DIR / "dwd" / "charge_order"
    write_csv(dwd, dest)
    print("DWD rows: %s -> %s  file: %s" % (before, after, dest / "charge_order.csv"))
    if after >= before:
        print("WARN: 清洗后行数未减少，请检查迷你 ODS 脏数据是否读到")


def not_ready(stage: str) -> None:
    print(
        "stage=%s 尚未实现。qa/clean 通了之后再补 SparkSQL DWS/ADS 与 MLlib。"
        % stage
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="charging-system Spark 流水线")
    parser.add_argument(
        "--stage",
        required=True,
        choices=["qa", "clean", "dws", "ads", "train"],
        help="qa=探查 clean=清洗 dws/ads=SparkSQL train=MLlib",
    )
    args = parser.parse_args()

    spark = spark_session("charging-pipeline-" + args.stage)
    try:
        if args.stage == "qa":
            stage_qa(spark)
        elif args.stage == "clean":
            stage_clean(spark)
        else:
            not_ready(args.stage)
    finally:
        spark.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
