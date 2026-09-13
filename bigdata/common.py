"""bigdata/ 下所有脚本共用的底座：数据库连接 + CSV 输出约定。

数据库口令沿用服务端 server/config.h 的约定：优先读环境变量
CHARGING_DB_PASSWORD，没设时回退到实训默认值（该默认值在
server/config.h、README.md 里本来就有，不是新增的泄漏面）。
"""

import csv
import os
from datetime import date, datetime
from decimal import Decimal
from pathlib import Path

import pymysql

ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "out"

DB = {
    "host": "127.0.0.1",
    "port": 3306,
    "user": "charging_user",
    "password": os.environ.get("CHARGING_DB_PASSWORD", "123456"),
    "database": "charging_system",
    "charset": "utf8mb4",
}


def connect():
    """连演示库。调用方负责 close()。"""
    return pymysql.connect(**DB)


def _fmt(v):
    """把 Python 值转成 CSV 字段。

    DECIMAL 用 format(v, "f") 而不是 str(v)，避免出现科学计数法；
    时间统一成 MySQL 的 YYYY-MM-DD HH:MM:SS。
    """
    if isinstance(v, datetime):
        return v.strftime("%Y-%m-%d %H:%M:%S")
    if isinstance(v, date):
        return v.strftime("%Y-%m-%d")
    if isinstance(v, Decimal):
        return format(v, "f")
    return v


def write_csv(path, columns, rows):
    """写一个 CSV 文件，返回写出的数据行数。

    约定（Spark 端读取时依赖这些）：
      - UTF-8 无 BOM，行尾 \\n
      - 首行表头，列名 = 数据库列名，顺序 = 表定义顺序
      - NULL 输出为空字段
    """
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(columns)
        for row in rows:
            w.writerow(["" if v is None else _fmt(v) for v in row])
    return len(rows)
