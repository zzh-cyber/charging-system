#!/usr/bin/env python3
"""NO.111 验收：Spark 从 HDFS 读 ODS 三个 CSV，逐条核对 `dq_expected.json` 契约。

边界说明：这是**邓雅心这一侧（模拟数据 / 脏数据 / 上 HDFS）的验收脚本**，
不是流水线。NO.113~115 的探查、清洗、DWS 归翟梓涵，文件名和入口都由他定。
第 [7] 段在 Spark 里把矩阵的四条清洗规则跑了一遍，只是用来从下游方向
**反证契约自洽**（dwd_rows / dwd_kwh_total 对得上），不是替他实现 DWD。

    source ~/.hadoop_env.sh
    spark-submit --master 'local[2]' bigdata/check_ods_hdfs.py

前置：演示库只读；Hadoop 已起；`put_ods.sh` 已把三份 CSV 传上 HDFS。
全过返回 0，任一条不过返回 1。
"""
import json
import sys
from decimal import Decimal

from pyspark.sql import SparkSession, functions as F
from pyspark.sql.window import Window

ODS = "hdfs://localhost:8020/user/charging/ods"

# 显式 schema：不用 inferSchema（空字段会被当空串塞进 string 列，数值列可能整列推错）
CHARGE_ORDER_SCHEMA = """
    id long, order_no string, user_id long, station_id long, pile_id long,
    status string, unit_price decimal(6,2), start_soc decimal(5,2),
    battery_capacity_kwh decimal(6,2), target_soc decimal(5,2),
    reserve_time timestamp, start_time timestamp, end_time timestamp,
    duration_seconds int, kwh decimal(10,2), amount decimal(10,2),
    pay_request_id string, created_at timestamp, updated_at timestamp,
    dq_tag string
"""

# ⚠️ 列名照抄数据库真实列名。注意 Spark 的 CSV reader 在【给了显式 schema 时是
#    按位置映射、不看表头名字】的 —— 所以列名写错不会报错，只会在你按名字取值时
#    静默取到隔壁列。这里是照 SHOW CREATE TABLE 抄的，别凭印象改。
#    另注：station 没有 total_piles 列，桩数要从 pile 文件按 station_id 数。
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

problems = []


def check(ok, label, detail=""):
    print(f"  {'✅' if ok else '❌'} {label}{'  ' + detail if detail else ''}")
    if not ok:
        problems.append(label)


def main():
    expected = json.load(open(
        "/home/dyx/charging-system/bigdata/dq_expected.json", encoding="utf-8"))

    spark = (SparkSession.builder
             .appName("verify-ods-hdfs")
             # ⚠️ 这条不加：容器按 UTC 解析，hour() 整体挪 8 小时，
             #    早高峰 8-9 会变成 0-1，大屏曲线整个错位
             .config("spark.sql.session.timeZone", "Asia/Shanghai")
             .getOrCreate())
    spark.sparkContext.setLogLevel("WARN")

    print("=" * 68)
    print("NO.111 回读验收：Spark 读 HDFS 上的 ODS")
    print("=" * 68)

    # ---------------- charge_order ----------------
    print("\n[1] charge_order")
    order = (spark.read.option("header", True).option("nullValue", "")
             .schema(CHARGE_ORDER_SCHEMA)
             .csv(f"{ODS}/charge_order/ods_charge_order.csv"))

    n = order.count()
    check(n == expected["expected_probe"]["ods_rows"],
          f"行数 {n}", f"(契约 {expected['expected_probe']['ods_rows']})")
    check(len(order.columns) == 20, f"列数 {len(order.columns)}", "(契约 20)")

    # 表头完整性：19 列业务列 + dq_tag
    check(order.columns[-1] == "dq_tag", "末列是 dq_tag", order.columns[-1])
    check(order.columns[:19] == [
        "id", "order_no", "user_id", "station_id", "pile_id", "status",
        "unit_price", "start_soc", "battery_capacity_kwh", "target_soc",
        "reserve_time", "start_time", "end_time", "duration_seconds", "kwh",
        "amount", "pay_request_id", "created_at", "updated_at"],
        "前 19 列表头与 charge_order 定义一致")

    # ---------------- dq_tag 分布 == 注入数 ----------------
    print("\n[2] dq_tag 分布 vs 契约 injected")
    dist = {r["dq_tag"]: r["c"] for r in
            order.groupBy("dq_tag").count()
              .withColumnRenamed("count", "c").collect()}
    check(dist.get("OK") == expected["derived"]["clean_rows"],
          f"OK 干净行 {dist.get('OK')}",
          f"(契约 {expected['derived']['clean_rows']})")
    for tag, want in expected["injected"].items():
        if tag.startswith("_"):
            continue
        got = dist.get(tag, 0)
        check(got == want, f"{tag:<17}{got:>5}", f"(契约 {want})")
    dirty_got = n - dist.get("OK", 0)
    check(dirty_got == expected["derived"]["dirty_rows"],
          f"脏行合计 {dirty_got}", f"(契约 {expected['derived']['dirty_rows']})")

    # ---------------- 时区：早/晚高峰确实在 8-9 / 17-20 ----------------
    print("\n[3] 小时分布（timeZone=Asia/Shanghai）")
    hours = {r["h"]: r["c"] for r in
             order.groupBy(F.hour("start_time").alias("h")).count()
                 .withColumnRenamed("count", "c").collect()
             if r["h"] is not None}
    for h in range(24):
        c = hours.get(h, 0)
        bar = "█" * (c * 40 // max(1, max(hours.values())))
        mark = "  ← 早高峰" if h in (8, 9) else ("  ← 晚高峰" if h in (17, 18, 19, 20) else "")
        print(f"     {h:02d} {c:>5} {bar}{mark}")

    morning = hours.get(8, 0) + hours.get(9, 0)
    evening = sum(hours.get(h, 0) for h in (17, 18, 19, 20))
    night = sum(hours.get(h, 0) for h in range(0, 6))
    check(morning > night * 3, f"早高峰 8-9 共 {morning} 单 >> 凌晨 0-5 的 {night} 单")
    check(evening > night * 4, f"晚高峰 17-20 共 {evening} 单")
    check(hours.get(8, 0) > 0 and hours.get(9, 0) > 0,
          "8 点和 9 点都有单（若设成 UTC，峰值会掉到 0-1 点）")

    # ---------------- 反证：解析/渲染时区不一致会挪 8 小时 ----------------
    # 注意：直接 groupBy 是【看不出来】的 —— CSV 里是无时区的裸时间串，
    # 重新读一遍会在新时区里重新解析，解析与渲染同时挪、正好抵消。
    # 必须先 cache（等价于「落盘后换个会话读」），才暴露出真实位移。
    print("\n[4] 反证：解析后换渲染时区 → 8 小时位移")
    order.cache()
    order.count()                                  # 先按上海解析并固化
    spark.conf.set("spark.sql.session.timeZone", "UTC")
    hours_utc = {r["h"]: r["c"] for r in
                 order.groupBy(F.hour("start_time").alias("h")).count()
                 .withColumnRenamed("count", "c").collect()
                 if r["h"] is not None}
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    order.unpersist()
    shift_ok = all(
        hours_utc.get((h - 8) % 24, 0) == hours.get(h, 0) for h in range(24))
    check(shift_ok, "每个小时桶整体挪了 8 位",
          f"上海 8 点 {hours.get(8)} → UTC 0 点 {hours_utc.get(0)}")
    check(hours_utc.get(8, 0) == hours.get(16, 0),
          "UTC 下的 8 点 == 上海的 16 点（曲线整体左移）")
    print("     ⇒ 所以时区必须在【每个作业】里设成同一个值。")
    print("       不一致的后果不在这一次 hour()，而在：DWS 落 parquet 用一个时区、")
    print("       ADS 读用另一个时区；或拿裸时间串和 current_timestamp() 比大小。")

    # ---------------- 干净行自洽 ----------------
    print("\n[5] 干净行自洽（dq_tag='OK'，另排掉跨 sim_now 被截断的会话）")
    clean = order.filter(F.col("dq_tag") == "OK")
    # 跨过 sim_now 的会话按已充时长折算过 kwh，本来就不满足整段的关系式
    truncated = clean.filter(F.col("end_time") == F.lit("2026-09-13 21:00:00"))
    nt = truncated.count()
    full = clean.filter(F.col("end_time") != F.lit("2026-09-13 21:00:00"))
    print(f"     （排掉 {nt} 行被截断的 charging 会话，其 kwh 只算已充部分）")
    check(truncated.filter(F.col("status") != "charging").count() == 0,
          "被截断的行 status 都是 charging")

    bad = full.filter(
        F.abs(F.col("kwh") - (F.col("target_soc") - F.col("start_soc")) / 100
              * F.col("battery_capacity_kwh")).cast("double").cast("decimal(10,2)")
        > F.lit(Decimal("0.02")).cast("decimal(10,2)"))
    check(bad.count() == 0, "kwh == (target-start)/100*capacity",
          f"偏差>0.02 的行数 {bad.count()}")

    bad2 = clean.filter(F.col("kwh") > F.col("battery_capacity_kwh"))
    check(bad2.count() == 0, "kwh <= battery_capacity_kwh",
          f"越界行数 {bad2.count()}")

    bad3 = clean.filter(
        F.col("duration_seconds") !=
        F.unix_timestamp("end_time") - F.unix_timestamp("start_time"))
    check(bad3.count() == 0, "duration_seconds == end_time - start_time",
          f"对不上 {bad3.count()} 行")

    bad4 = clean.filter(F.col("end_time") > F.lit("2026-09-13 21:00:00"))
    check(bad4.count() == 0, "end_time <= sim_now（无未来的负荷）",
          f"越界 {bad4.count()} 行")

    # ---------------- 维表 ----------------
    print("\n[6] 维表")
    station = (spark.read.option("header", True).option("nullValue", "")
               .schema(STATION_SCHEMA).csv(f"{ODS}/station/ods_station.csv"))
    pile = (spark.read.option("header", True).option("nullValue", "")
            .schema(PILE_SCHEMA).csv(f"{ODS}/pile/ods_pile.csv"))
    ns, np_ = station.count(), pile.count()
    dim = expected["dimension"]
    check(ns == dim["stations"], f"station {ns} 行", f"(契约 {dim['stations']})")
    check(np_ == dim["piles"], f"pile {np_} 行", f"(契约 {dim['piles']})")
    check(73 not in [r["id"] for r in station.select("id").collect()], "不含站 73")
    bad5 = clean.join(station, clean.station_id == station.id, "left_anti").count()
    check(bad5 == 0, "干净行的 station_id 都能 join 上", f"孤儿 {bad5} 行")
    bad6 = clean.join(pile, clean.pile_id == pile.id, "left_anti").count()
    check(bad6 == 0, "干净行的 pile_id 都能 join 上", f"孤儿 {bad6} 行")

    # ---------------- 端到端：在 Spark 里按矩阵四条规则算出 DWD ----------------
    # 这一段就是翟梓涵 NO.114 要写的东西，提前跑通一遍，等于验证契约本身。
    # 注意 dq_tag 在这里【完全没被用到】—— 清洗必须是按规则真清洗，
    # 拿注入标签当清洗依据是自证。
    print("\n[7] 按矩阵四条规则在 Spark 里清洗，对契约的 dwd_rows / dwd_kwh_total")
    kept = order.filter(
        F.col("start_time").isNotNull()
        & F.col("kwh").isNotNull()
        & (F.col("kwh") >= 0)
        & ~(F.col("end_time") < F.col("start_time")))
    kept = kept.join(station.select("id"), kept.station_id == station.id, "left_semi")
    kept = kept.join(pile.select("id"), kept.pile_id == pile.id, "left_semi")

    w = Window.partitionBy("order_no").orderBy(
        F.col("updated_at").desc(), F.col("created_at").desc())
    dwd = (kept.withColumn("_rn", F.row_number().over(w))
           .filter(F.col("_rn") == 1).drop("_rn"))

    nd = dwd.count()
    want_n = expected["derived"]["dwd_rows"]
    check(nd == want_n, f"DWD 行数 {nd}", f"(契约 {want_n})")

    total = dwd.agg(F.sum("kwh")).collect()[0][0]
    want_k = Decimal(expected["derived"]["dwd_kwh_total"])
    check(abs(Decimal(str(total)) - want_k) < Decimal("0.5"),
          f"DWD kwh 合计 {total}", f"(契约 {want_k})")
    print("     ⚠️ SOC_RANGE 的 177 行【必须留在这里】—— 矩阵口径是「裁剪或置空」，")
    print("        不是丢弃；丢掉的话 DWD 会少 177 行、少约 12457 度，对不上契约。")

    # dq_tag 在 DWD 之后必须 drop（这里只是证明它没参与清洗）
    check("dq_tag" in dwd.columns, "dq_tag 此刻还在（DWD 收尾要 drop 掉它）")

    spark.stop()

    print("\n" + "=" * 68)
    if problems:
        print(f"❌ {len(problems)} 项未通过：")
        for p in problems:
            print(f"   - {p}")
        return 1
    print("✅ NO.111 回读验收全部通过")
    print("=" * 68)
    return 0


if __name__ == "__main__":
    sys.exit(main())
