#!/usr/bin/env python3
"""生成 ODS 层数据：维表导出（NO.108）+ 模拟订单与脏数据（NO.109 / NO.110）。

只读演示库，绝不写入 —— 全程只有 SELECT。

用法（系统 python3，与 tests/*.py 一致，pymysql 已装）：
    python3 bigdata/gen_ods.py                 # 只导维表（NO.108）
    python3 bigdata/gen_ods.py --orders        # 维表 + 模拟订单（NO.109/110）
    python3 bigdata/gen_ods.py --orders --rows 20000 --days 14 --seed 20260913
    python3 bigdata/gen_ods.py --tables station
    python3 bigdata/gen_ods.py --keep-all-stations   # 不排除站 73

可复现：同一组 (--seed --end-date --sim-now --days --rows --dirty-ratio) 下
输出逐字节相同。解析后的参数会写进 bigdata/dq_expected.json 备查。
"""

import argparse
import json
import sys
from datetime import date, datetime
from decimal import Decimal
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common      # noqa: E402
import simulate    # noqa: E402

#: ODS 维表：只导出这两张，保持干净（脏数据只在订单上，见 NO.110）
TABLES = ("station", "pile")

#: 默认排除的站。73 号是演示库里测试建的站：station_code 是 UUID、名字叫
#: 「1」、单价 2.00。它带着 6 个空闲桩而占用率为 0，会让矩阵里的
#: recommend_score = idle×10 − util = 60 排到用户端「智能推荐」的最前面，
#: 把演示用户导到一个测试站去。排除后维表正好是 72 站 / 399 桩，
#: 与需求矩阵 NO.60a 的「约 72 座站、约 399 根桩」对上。
DEFAULT_EXCLUDE_STATIONS = [73]

#: 契约文件放在 bigdata/ 而不是 out/ —— out/ 被 .gitignore 整个忽略，
#: 放那儿两位队友就看不到这份接口约定了。
CONTRACT_PATH = common.ROOT / "dq_expected.json"

#: 默认窗口 30 天而不是矩阵下限的 14 天，是实测调出来的：
#: 72 站 / 399 桩 / 14 天的容量装不下 2 万单 —— 实测 14 天时有 17.7% 的
#: 站×小时占用率 ≥80%、72 个站有 70 个会触顶，等于全城永远堵着，
#: 热点失去意义、congestion 全变 high。放到 30 天后：
#:   桩平均工时占用率 10.3%（真实演示库此刻是 15.6%，量级一致）
#:   晚高峰平均每天 14.9 个站 ≥80%，深夜只有 0.1 个 —— 昼夜节律是真的
#: 想要别的密度就调 --days / --rows，不用改代码。


def table_columns(conn, table):
    """从 information_schema 取列名，按真实定义顺序。

    用来独立校验 cursor.description 给出的列序，防止驱动行为变化。
    """
    with conn.cursor() as cur:
        cur.execute(
            "SELECT column_name FROM information_schema.columns "
            "WHERE table_schema = DATABASE() AND table_name = %s "
            "ORDER BY ordinal_position",
            (table,),
        )
        return [r[0] for r in cur.fetchall()]


def fetch_rows(conn, table, excluded_stations):
    """取一张表的全部行。station 按 id 过滤，pile 按 station_id 过滤。

    维表【本身保持干净】—— 脏数据只注入到订单上。
    """
    if excluded_stations and table == "station":
        where, args = " WHERE id NOT IN (%s)" % ",".join(["%s"] * len(excluded_stations)), list(excluded_stations)
    elif excluded_stations and table == "pile":
        where, args = " WHERE station_id NOT IN (%s)" % ",".join(["%s"] * len(excluded_stations)), list(excluded_stations)
    else:
        where, args = "", []

    with conn.cursor() as cur:
        cur.execute(f"SELECT * FROM `{table}`{where}", args)
        columns = [d[0] for d in cur.description]
        rows = cur.fetchall()
        cur.execute(f"SELECT COUNT(*) FROM `{table}`{where}", args)
        count = cur.fetchone()[0]

    expected = table_columns(conn, table)
    if columns != expected:
        raise RuntimeError(
            f"{table} 列序不一致：结果集 {columns} != information_schema {expected}")
    if len(rows) != count:
        raise RuntimeError(f"{table} 行数对不上：取到 {len(rows)}，库里 {count}")
    return columns, rows


def export_table(conn, table, out_dir, excluded_stations):
    columns, rows = fetch_rows(conn, table, excluded_stations)
    path = out_dir / f"ods_{table}.csv"
    written = common.write_csv(path, columns, rows)
    return path, written, columns


def load_dimensions(conn, excluded_stations):
    """给生成器准备维表对象。与上面导 CSV 用的是同一套过滤条件 ——
    两边必须一致，否则过滤后 400~405 会静默变成孤儿桩，把计数顶超。"""
    _, srows = fetch_rows(conn, "station", excluded_stations)
    _, prows = fetch_rows(conn, "pile", excluded_stations)

    stations = [simulate.Station(id=r[0], code=r[1], name=r[2], price=Decimal(str(r[6])))
                for r in srows]
    piles = [simulate.Pile(id=r[0], station_id=r[1], code=r[2], type=r[3],
                           power_kw=Decimal(str(r[4])))
             for r in prows]

    with conn.cursor() as cur:
        cur.execute("SELECT id FROM user ORDER BY id")
        users = [r[0] for r in cur.fetchall()]
    if not users:
        raise RuntimeError("演示库里没有用户，订单的 user_id 无从取起")
    return stations, piles, users


def main():
    ap = argparse.ArgumentParser(
        description="生成 ODS 层数据（维表导出 + 模拟订单 + 脏数据注入）")
    ap.add_argument("--tables", nargs="+", default=list(TABLES),
                    choices=list(TABLES), help="要导出的维表（默认全部）")
    ap.add_argument("--out-dir", default=str(common.OUT_DIR),
                    help=f"输出目录（默认 {common.OUT_DIR}）")
    ap.add_argument("--orders", action="store_true",
                    help="同时生成模拟订单（NO.109/NO.110）")
    ap.add_argument("--days", type=int, default=30,
                    help="订单时间窗天数，14~30（默认 30，见下方注释）")
    ap.add_argument("--rows", type=int, default=20000,
                    help="订单总行数，含脏行，1~3 万（默认 20000）")
    ap.add_argument("--seed", type=int, default=20260913, help="随机种子")
    ap.add_argument("--end-date", default=None,
                    help="窗口最后一天 YYYY-MM-DD（默认今天）")
    ap.add_argument("--sim-now", default="21:00:00",
                    help="「模拟当前时刻」，落在 end-date 当天（默认 21:00:00）")
    ap.add_argument("--dirty-ratio", type=float, default=0.08,
                    help="脏数据比例，0.05~0.10（默认 0.08）")
    ap.add_argument("--exclude-stations", type=int, nargs="*",
                    default=DEFAULT_EXCLUDE_STATIONS,
                    help=f"排除的站 id（默认 {DEFAULT_EXCLUDE_STATIONS}）")
    ap.add_argument("--keep-all-stations", action="store_true",
                    help="不排除任何站（逃生舱）")
    ap.add_argument("--holidays", nargs="*", default=[],
                    help="窗口内的节假日 YYYY-MM-DD（默认无）")
    args = ap.parse_args()

    excluded = [] if args.keep_all_stations else args.exclude_stations
    out_dir = Path(args.out_dir)

    print("=" * 68)
    print("ODS 生成")
    print("=" * 68)
    print(f"源库: {common.DB['user']}@{common.DB['host']}:{common.DB['port']}"
          f"/{common.DB['database']}")
    print(f"输出: {out_dir}")
    if excluded:
        print(f"排除站: {excluded}（测试站，见脚本头部注释）")
    print()

    conn = common.connect()
    rc = 0
    try:
        total = 0
        for table in args.tables:
            path, n, columns = export_table(conn, table, out_dir, excluded)
            total += n
            print(f"✅ {table:8s} {n:5d} 行, {len(columns):2d} 列 -> {path.name}")
            print(f"   列: {', '.join(columns)}")
        print()
        print(f"维表共 {total} 行导出到 {out_dir}")

        if not args.orders:
            print("=" * 68)
            print("（未加 --orders，只导了维表。要造订单加 --orders）")
            return 0

        print()
        print("-" * 68)
        print("模拟订单生成")
        print("-" * 68)
        rc = generate_orders(conn, args, out_dir, excluded)
    except Exception as e:
        print(f"\n❌ 失败: {e}", file=sys.stderr)
        return 1
    finally:
        conn.close()
    return rc


def generate_orders(conn, args, out_dir, excluded):
    end_date = (date.fromisoformat(args.end_date) if args.end_date else date.today())
    hh, mm, ss = (int(x) for x in args.sim_now.split(":"))
    sim_now = datetime.combine(end_date, datetime.min.time()).replace(
        hour=hh, minute=mm, second=ss)
    holidays = frozenset(date.fromisoformat(d) for d in args.holidays)

    params = simulate.Params(
        seed=args.seed, days=args.days, rows=args.rows,
        end_date=end_date, sim_now=sim_now,
        dirty_ratio=args.dirty_ratio, holidays=holidays,
    )

    stations, piles, users = load_dimensions(conn, excluded)
    print(f"维表: {len(stations)} 站 / {len(piles)} 桩 / {len(users)} 用户")
    print(f"窗口: {params.start_date} ~ {params.end_date}"
          f"（{params.days} 天），模拟当前时刻 {sim_now}")
    print()

    rows, report = simulate.generate(stations, piles, users, params)

    # —— 核心自检：注入数 == 探查数。对不上就不写文件，非 0 退出 ——
    problems = simulate.verify(rows, report)
    if problems:
        print("❌ 自检未通过，未写出任何订单文件：", file=sys.stderr)
        for p in problems:
            print(f"   - {p}", file=sys.stderr)
        return 1

    path = out_dir / "ods_charge_order.csv"
    written = common.write_csv(path, simulate.COLUMNS,
                              [[r[c] for c in simulate.COLUMNS] for r in rows])
    if written != args.rows:
        print(f"❌ 行数对不上：写出 {written}，期望 {args.rows}", file=sys.stderr)
        return 1

    print(f"✅ 订单 {written} 行, {len(simulate.COLUMNS)} 列 -> {path.name}")
    print(f"   列: {', '.join(simulate.COLUMNS)}")
    print()
    print_injection_table(report)

    CONTRACT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2, default=str) + "\n",
        encoding="utf-8")
    print()
    print(f"✅ 契约 -> {CONTRACT_PATH}")
    print("   （放 bigdata/ 不放 out/，out/ 被 .gitignore 忽略，队友看不到）")
    print("=" * 68)
    return 0


def print_injection_table(report):
    """把注入数与期望探查数并排打出来 —— 这就是 9/14 验收要对的那张表。"""
    inj = report["injected"]
    probe = report["expected_probe"]
    deriv = report["derived"]

    pairs = [
        ("NULL_START_TIME", "null_start_time", True),
        ("NULL_KWH", "null_kwh", False),
        ("NEG_KWH", "negative_kwh", False),
        ("SOC_RANGE", "soc_range", False),
        ("TIME_REVERSED", "time_reversed", False),
        ("DUP_ORDER_NO", "dup_order_no", False),
        ("ORPHAN_STATION", "orphan_station", False),
        ("ORPHAN_PILE", "orphan_pile", False),
        ("STATUS_CONFLICT", "status_conflict", False),
    ]
    print("  注入数 == 期望探查数（9/14 拿这张表对 NO.113 的报告）")
    print("  " + "-" * 62)
    print(f"  {'dq_tag':<17}{'注入':>6}{'业务天然':>10}{'期望探查':>10}")
    print("  " + "-" * 62)
    for tag, key, natural in pairs:
        nat = deriv["natural_null_start"] if natural else 0
        print(f"  {tag:<17}{inj[tag]:>6}{nat:>10}{probe[key]:>10}")
    print("  " + "-" * 62)
    print(f"  {'合计脏行':<17}{inj['_total_dirty_rows']:>6}")
    print()
    print(f"  ODS 行数        {probe['ods_rows']}")
    print(f"  干净行          {deriv['clean_rows']}")
    print(f"  脏行            {deriv['dirty_rows']}"
          f"  （占 {deriv['dirty_rows'] / probe['ods_rows']:.1%}）")
    print(f"  期望 DWD 行数   {deriv['dwd_rows']}")
    print()
    print("  注：null_start_time 还含业务上天然未开始的行"
          f"（reserved / 未开始的 cancelled 共 {deriv['natural_null_start']} 行），")
    print("      它们不是脏数据，但 isNull(start_time) 一样会数到 —— 契约里两者分开写。")


if __name__ == "__main__":
    sys.exit(main())
