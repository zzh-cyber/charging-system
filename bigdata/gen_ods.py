#!/usr/bin/env python3
"""从演示库导出 ODS 维表 CSV（NO.108）。

只读演示库，绝不写入。列名与顺序直接取自结果集，因此永远与
sql/schema.sql 的表定义对齐；脚本另外拿 information_schema 的列序
做一次独立校验。

用法（用系统 python3，与 tests/*.py 一致，pymysql 已装）：
    python3 bigdata/gen_ods.py
    python3 bigdata/gen_ods.py --tables station
    python3 bigdata/gen_ods.py --out-dir /tmp/ods
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402

# ODS 维表：只导出这两张，保持干净（脏数据只在订单上，见 NO.110）
TABLES = ("station", "pile")


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


def export_table(conn, table, out_dir):
    """导出一张表，返回 (csv 路径, 行数)。"""
    with conn.cursor() as cur:
        cur.execute(f"SELECT * FROM `{table}`")
        columns = [d[0] for d in cur.description]
        rows = cur.fetchall()

        cur.execute(f"SELECT COUNT(*) FROM `{table}`")
        db_count = cur.fetchone()[0]

    # 自检 1：结果集列序 == information_schema 列序
    expected = table_columns(conn, table)
    if columns != expected:
        raise RuntimeError(
            f"{table} 列序不一致：结果集 {columns} != information_schema {expected}"
        )

    path = out_dir / f"ods_{table}.csv"
    written = common.write_csv(path, columns, rows)

    # 自检 2：写出的行数 == 库里行数
    if written != db_count:
        raise RuntimeError(f"{table} 行数对不上：写出 {written}，库里 {db_count}")

    return path, written, columns


def main():
    ap = argparse.ArgumentParser(description="从演示库导出 ODS 维表 CSV")
    ap.add_argument("--tables", nargs="+", default=list(TABLES),
                    choices=list(TABLES), help="要导出的表（默认全部）")
    ap.add_argument("--out-dir", default=str(common.OUT_DIR),
                    help=f"输出目录（默认 {common.OUT_DIR}）")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    print("=" * 60)
    print("ODS 维表导出")
    print("=" * 60)
    print(f"源库: {common.DB['user']}@{common.DB['host']}:{common.DB['port']}"
          f"/{common.DB['database']}")
    print(f"输出: {out_dir}")
    print()

    conn = common.connect()
    try:
        total = 0
        for table in args.tables:
            path, n, columns = export_table(conn, table, out_dir)
            total += n
            print(f"✅ {table:8s} {n:5d} 行, {len(columns):2d} 列 -> {path.name}")
            print(f"   列: {', '.join(columns)}")
        print()
        print(f"共 {total} 行导出到 {out_dir}")
        print("=" * 60)
    except Exception as e:
        print(f"\n❌ 导出失败: {e}", file=sys.stderr)
        return 1
    finally:
        conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
