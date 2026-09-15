#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用户群体运营大屏 API（/user-groups）。

查演示库 MySQL，不走 Spark / pipeline.py，也不改 TCP MsgType。
管理员 Bearer 用现有 admin_user_list 验票（SessionManager 在充电服务进程内）。
"""
from __future__ import annotations

import json
import os
import socket
import struct
from collections import Counter, defaultdict
from datetime import date, datetime, time, timedelta
from decimal import Decimal, ROUND_HALF_UP
from functools import wraps

import pymysql
from flask import Blueprint, jsonify, request
from pymysql.cursors import DictCursor

bp = Blueprint("user_groups", __name__, url_prefix="/api/admin/user-groups")

TCP_HOST = os.environ.get("CHARGING_SERVER_HOST", "127.0.0.1")
TCP_PORT = int(os.environ.get("CHARGING_SERVER_PORT", "9000"))
TCP_TIMEOUT = float(os.environ.get("CHARGING_SERVER_TIMEOUT", "3"))

DB = {
    "host": os.environ.get("CHARGING_DB_HOST", "127.0.0.1"),
    "port": int(os.environ.get("CHARGING_DB_PORT", "3306")),
    "user": os.environ.get("CHARGING_DB_USER", "charging_user"),
    "password": os.environ.get("CHARGING_DB_PASSWORD", "123456"),
    "database": os.environ.get("CHARGING_DB_NAME", "charging_system"),
    "charset": "utf8mb4",
    "cursorclass": DictCursor,
    "autocommit": False,
}

PERIODS = ("凌晨", "上午", "下午", "晚间")
FREQ_ORDER = ("high", "medium", "low", "inactive")
VALUE_ORDER = ("high", "medium", "low")
FREQ_PIE = {
    "high": "高频用户",
    "medium": "中频用户",
    "low": "低频用户",
    "inactive": "沉默用户",
}
FREQ_SHORT = {"high": "高频", "medium": "中频", "low": "低频", "inactive": "沉默"}
VALUE_PIE = {"high": "高价值", "medium": "中价值", "low": "低价值"}
PILE_LABEL = {"fast": "快充", "slow": "慢充", "mixed": "混合"}
AGE_LABEL = {
    "under_25": "25岁以下",
    "25_34": "25-34岁",
    "35_44": "35-44岁",
    "45_54": "45-54岁",
    "55_plus": "55岁以上",
    "unknown": "未知",
}
SOURCE_LABEL = {
    "android": "Android",
    "ios": "iOS",
    "web": "Web",
    "qt": "Qt",
    "offline": "线下",
    "unknown": "未知",
}
AGE_KEYS = ("under_25", "25_34", "35_44", "45_54", "55_plus", "unknown")
SOURCE_KEYS = ("android", "ios", "web", "qt", "offline", "unknown")


def money(v) -> float:
    if v is None:
        return 0.0
    return float(Decimal(str(v)).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP))


def frequency_of(n30: int) -> str:
    if n30 >= 8:
        return "high"
    if n30 >= 3:
        return "medium"
    if n30 >= 1:
        return "low"
    return "inactive"


def value_level_from_rank(rank: int, n: int) -> str:
    """全体人名次：前 20% high，20~70% medium，后 30% low。"""
    if n <= 0 or rank <= 0:
        return "low"
    high_end = int(n * 0.20)
    medium_end = int(n * 0.70)
    if rank <= high_end:
        return "high"
    if rank <= medium_end:
        return "medium"
    return "low"


def period_of_hour(hour: int) -> str:
    if hour < 6:
        return "凌晨"
    if hour < 12:
        return "上午"
    if hour < 18:
        return "下午"
    return "晚间"


def preferred_pile_type(fast: int, total: int) -> str:
    if total <= 0:
        return "mixed"
    ratio = fast / total
    if ratio >= 0.60:
        return "fast"
    if ratio <= 0.40:
        return "slow"
    return "mixed"


def mask_phone(phone: str) -> str:
    p = phone or ""
    if len(p) >= 7:
        return f"{p[:3]}****{p[-4:]}"
    return p or "--"


def cutoff_30d(today: date | None = None) -> datetime:
    today = today or date.today()
    return datetime.combine(today - timedelta(days=30), time.min)


def now_str(now: datetime | None = None) -> str:
    return (now or datetime.now()).strftime("%Y-%m-%d %H:%M:%S")


def _as_dt(v):
    if v is None or isinstance(v, datetime):
        return v
    if isinstance(v, date):
        return datetime.combine(v, time.min)
    return None


def _order_hour(row: dict) -> int | None:
    for key in ("start_time", "end_time", "created_at"):
        dt = _as_dt(row.get(key))
        if dt is not None:
            return dt.hour
    return None


# ---------- TCP 验管理员 token（现有 admin_user_list，不新开 MsgType） ----------


def _recv_n(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("充电服务关闭连接")
        buf += chunk
    return buf


def tcp_request(obj: dict) -> dict:
    payload = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    with socket.create_connection((TCP_HOST, TCP_PORT), timeout=TCP_TIMEOUT) as sock:
        sock.sendall(struct.pack(">I", len(payload)) + payload)
        size = struct.unpack(">I", _recv_n(sock, 4))[0]
        if size > 2_000_000:
            raise ValueError("TCP 响应过长")
        return json.loads(_recv_n(sock, size).decode("utf-8"))


def validate_admin_token(token: str) -> tuple[bool, int, str]:
    try:
        resp = tcp_request(
            {
                "type": "admin_user_list",
                "token": token,
                "data": {"page": 1, "page_size": 1},
            }
        )
    except OSError:
        return False, 401, "充电服务不可用，无法校验管理员身份"
    code = resp.get("code")
    if code == 0:
        return True, 200, ""
    if code == 9:
        return False, 403, "当前账号无管理权限"
    return False, 401, resp.get("msg") or "管理员登录已失效"


def _bearer_token() -> str:
    header = request.headers.get("Authorization") or ""
    if header.lower().startswith("bearer "):
        return header[7:].strip()
    return (request.headers.get("X-Admin-Token") or "").strip()


def require_admin(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
        token = _bearer_token()
        if not token:
            return jsonify({"code": 9, "msg": "管理员登录已失效"}), 401
        ok, status, msg = validate_admin_token(token)
        if not ok:
            return jsonify({"code": 9, "msg": msg}), status
        return fn(*args, **kwargs)

    return wrapper


def ok(data):
    return jsonify({"code": 0, "data": data})


def fail(msg: str, status: int = 500, code: int = 1):
    return jsonify({"code": code, "msg": msg}), status


# ---------- 画像计算 ----------


def _connect():
    return pymysql.connect(**DB)


def _fetch_users(cur) -> list[dict]:
    cur.execute(
        "SELECT id, phone, nickname, age_group, city, registration_source, "
        "created_at, last_active_at FROM `user` ORDER BY id"
    )
    return list(cur.fetchall())


def _fetch_settled(cur) -> list[dict]:
    cur.execute(
        "SELECT o.user_id, o.station_id, o.kwh, o.amount, o.duration_seconds, "
        "o.start_time, o.end_time, o.created_at, p.type AS pile_type "
        "FROM charge_order o "
        "JOIN pile p ON p.id = o.pile_id "
        "WHERE o.status = 'settled'"
    )
    return list(cur.fetchall())


def _fetch_stations(cur) -> dict[int, str]:
    cur.execute("SELECT id, name FROM station")
    return {int(r["id"]): r["name"] or f"站{r['id']}" for r in cur.fetchall()}


def build_profiles(users: list[dict], orders: list[dict], today: date | None = None) -> list[dict]:
    today = today or date.today()
    cutoff = cutoff_30d(today)
    acc: dict[int, dict] = {}
    for u in users:
        uid = int(u["id"])
        acc[uid] = {
            "user_id": uid,
            "phone": u.get("phone") or "",
            "nickname": u.get("nickname") or "",
            "age_group": u.get("age_group") or "unknown",
            "city": u.get("city") or "",
            "registration_source": u.get("registration_source") or "unknown",
            "created_at": u.get("created_at"),
            "last_active_at": u.get("last_active_at"),
            "total_orders": 0,
            "total_kwh": Decimal("0.00"),
            "total_amount": Decimal("0.00"),
            "duration_sum": 0,
            "orders_30d": 0,
            "kwh_30d": Decimal("0.00"),
            "amount_30d": Decimal("0.00"),
            "days_30d": set(),
            "periods": Counter(),
            "stations": Counter(),
            "fast": 0,
        }

    for row in orders:
        uid = int(row["user_id"])
        if uid not in acc:
            continue
        a = acc[uid]
        kwh = Decimal(str(row["kwh"] or 0))
        amount = Decimal(str(row["amount"] or 0))
        a["total_orders"] += 1
        a["total_kwh"] += kwh
        a["total_amount"] += amount
        a["duration_sum"] += int(row["duration_seconds"] or 0)
        if (row.get("pile_type") or "") == "fast":
            a["fast"] += 1
        hour = _order_hour(row)
        if hour is not None:
            a["periods"][period_of_hour(hour)] += 1
        sid = row.get("station_id")
        if sid is not None:
            a["stations"][int(sid)] += 1
        end = _as_dt(row.get("end_time"))
        if end is not None and end >= cutoff:
            a["orders_30d"] += 1
            a["kwh_30d"] += kwh
            a["amount_30d"] += amount
            a["days_30d"].add(end.date())

    ranked = sorted(
        acc.values(),
        key=lambda r: (-r["amount_30d"], r["user_id"]),
    )
    n = len(ranked)
    generated = now_str()
    out = []
    for i, a in enumerate(ranked, start=1):
        n30 = a["orders_30d"]
        freq = frequency_of(n30)
        total = a["total_orders"]
        period = a["periods"].most_common(1)[0][0] if a["periods"] else ""
        station_id = a["stations"].most_common(1)[0][0] if a["stations"] else None
        pile = preferred_pile_type(a["fast"], total)
        tags = [FREQ_SHORT[freq], VALUE_PIE[value_level_from_rank(i, n)]]
        out.append(
            {
                "user_id": a["user_id"],
                "phone": a["phone"],
                "nickname": a["nickname"] or f"用户{a['user_id']}",
                "age_group": a["age_group"],
                "city": a["city"],
                "registration_source": a["registration_source"],
                "created_at": a["created_at"],
                "last_active_at": a["last_active_at"],
                "total_orders": total,
                "total_kwh": money(a["total_kwh"]),
                "total_amount": money(a["total_amount"]),
                "avg_duration_seconds": (a["duration_sum"] // total) if total else 0,
                "orders_30d": n30,
                "kwh_30d": money(a["kwh_30d"]),
                "amount_30d": money(a["amount_30d"]),
                "active_days_30d": len(a["days_30d"]),
                "frequency_level": freq,
                "value_level": value_level_from_rank(i, n),
                "preferred_period": period,
                "preferred_station_id": station_id,
                "preferred_pile_type": pile,
                "fast_orders": a["fast"],
                "tags_json": json.dumps(tags, ensure_ascii=False),
                "generated_at": generated,
                "period_counts": a["periods"],
                "station_counts": a["stations"],
            }
        )
    return out


def persist_profiles(cur, profiles: list[dict], generated_at: str) -> None:
    sql = (
        "INSERT INTO user_group_profile ("
        "user_id, total_orders, total_kwh, total_amount, avg_duration_seconds, "
        "orders_30d, kwh_30d, amount_30d, active_days_30d, frequency_level, "
        "value_level, preferred_period, preferred_station_id, preferred_pile_type, "
        "tags_json, generated_at) VALUES ("
        "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s) "
        "ON DUPLICATE KEY UPDATE "
        "total_orders=VALUES(total_orders), total_kwh=VALUES(total_kwh), "
        "total_amount=VALUES(total_amount), "
        "avg_duration_seconds=VALUES(avg_duration_seconds), "
        "orders_30d=VALUES(orders_30d), kwh_30d=VALUES(kwh_30d), "
        "amount_30d=VALUES(amount_30d), active_days_30d=VALUES(active_days_30d), "
        "frequency_level=VALUES(frequency_level), value_level=VALUES(value_level), "
        "preferred_period=VALUES(preferred_period), "
        "preferred_station_id=VALUES(preferred_station_id), "
        "preferred_pile_type=VALUES(preferred_pile_type), "
        "tags_json=VALUES(tags_json), generated_at=VALUES(generated_at)"
    )
    rows = []
    for p in profiles:
        rows.append(
            (
                p["user_id"],
                p["total_orders"],
                p["total_kwh"],
                p["total_amount"],
                p["avg_duration_seconds"],
                p["orders_30d"],
                p["kwh_30d"],
                p["amount_30d"],
                p["active_days_30d"],
                p["frequency_level"],
                p["value_level"],
                p["preferred_period"],
                p["preferred_station_id"],
                p["preferred_pile_type"],
                p["tags_json"],
                generated_at,
            )
        )
    if rows:
        cur.executemany(sql, rows)


def build_insights(profiles: list[dict], period_share: list) -> list[dict]:
    n = len(profiles)
    items = []
    if n:
        inactive = sum(1 for p in profiles if p["frequency_level"] == "inactive")
        pct = round(inactive * 100.0 / n, 1)
        if pct >= 15:
            items.append(
                {
                    "insight_type": "wakeup",
                    "target_group": "inactive",
                    "icon": "⚠",
                    "title": "建议开展沉默用户唤醒",
                    "content": "可针对近30天未充电用户发放限时优惠券。",
                    "reason": f"沉默用户占比{pct}%",
                    "priority": 1,
                }
            )
        high_amt = sum(p["total_amount"] for p in profiles if p["value_level"] == "high")
        all_amt = sum(p["total_amount"] for p in profiles)
        if all_amt > 0:
            share = round(high_amt * 100.0 / all_amt, 1)
            if share >= 40:
                items.append(
                    {
                        "insight_type": "vip",
                        "target_group": "high",
                        "icon": "★",
                        "title": "加强高价值用户维护",
                        "content": "为高价值用户提供预约及积分权益。",
                        "reason": f"高价值用户贡献消费额{share}%",
                        "priority": 2,
                    }
                )
    evening = 0.0
    for row in period_share:
        name = row[0] if isinstance(row, (list, tuple)) else row.get("name")
        val = row[1] if isinstance(row, (list, tuple)) else row.get("value")
        if name == "晚间":
            evening = float(val or 0)
    if evening >= 35:
        items.append(
            {
                "insight_type": "peak_shift",
                "target_group": "evening",
                "icon": "◈",
                "title": "引导晚高峰用户错峰充电",
                "content": "建议将部分晚间订单引导至18点前。",
                "reason": f"晚间充电订单占比{evening}%",
                "priority": 2,
            }
        )
    items.sort(key=lambda x: (x["priority"], x["title"]))
    return items


def persist_insights(cur, items: list[dict], generated_at: str) -> None:
    cur.execute("DELETE FROM user_group_insight")
    if not items:
        return
    cur.executemany(
        "INSERT INTO user_group_insight "
        "(insight_type, target_group, title, content, reason, priority, generated_at) "
        "VALUES (%s,%s,%s,%s,%s,%s,%s)",
        [
            (
                x["insight_type"],
                x["target_group"],
                x["title"],
                x["content"],
                x["reason"],
                x["priority"],
                generated_at,
            )
            for x in items
        ],
    )


def _period_share(orders: list[dict]) -> list[dict]:
    counts = Counter()
    for row in orders:
        hour = _order_hour(row)
        if hour is not None:
            counts[period_of_hour(hour)] += 1
    total = sum(counts.values())
    out = []
    for name in PERIODS:
        pct = round(counts[name] * 100.0 / total, 1) if total else 0.0
        out.append({"name": name, "value": pct, "ratio": pct})
    return out


def _station_top(orders: list[dict], names: dict[int, str], limit: int = 6) -> list[dict]:
    users_by_station: dict[int, set] = defaultdict(set)
    for row in orders:
        sid = row.get("station_id")
        if sid is None:
            continue
        users_by_station[int(sid)].add(int(row["user_id"]))
    ranked = sorted(users_by_station.items(), key=lambda kv: (-len(kv[1]), kv[0]))[:limit]
    return [
        {
            "station_id": sid,
            "name": names.get(sid, f"站{sid}"),
            "station_name": names.get(sid, f"站{sid}"),
            "user_count": len(uids),
            "count": len(uids),
        }
        for sid, uids in ranked
    ]


def _attr_bars(profiles: list[dict], field: str, labels: dict, keys: tuple | None = None):
    counts = Counter()
    for p in profiles:
        raw = p.get(field) or ""
        if field == "city":
            raw = raw.strip() or "其他"
            counts[raw] += 1
        else:
            counts[raw] += 1
    if field == "city":
        items = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))[:8]
        return [{"name": k, "value": v} for k, v in items]
    out = []
    for key in keys or ():
        out.append({"name": labels.get(key, key), "value": counts.get(key, 0)})
    return out


def _overview(profiles: list[dict], today: date | None = None) -> dict:
    today = today or date.today()
    cutoff = cutoff_30d(today)
    n = len(profiles)
    inactive = sum(1 for p in profiles if p["frequency_level"] == "inactive")
    new_users = 0
    for p in profiles:
        created = _as_dt(p.get("created_at"))
        if created is not None and created >= cutoff:
            new_users += 1
    return {
        "total_users": n,
        "active_users_30d": n - inactive,
        "new_users_30d": new_users,
        "inactive_users_30d": inactive,
        "total_orders": int(sum(p["total_orders"] for p in profiles)),
        "total_kwh": money(sum(p["total_kwh"] for p in profiles)),
        "total_amount": money(sum(p["total_amount"] for p in profiles)),
    }


def _dist(profiles: list[dict], field: str, order: tuple, names: dict) -> list[dict]:
    counts = Counter(p[field] for p in profiles)
    return [
        {"name": names[k], "value": counts.get(k, 0), "level": k, "key": k}
        for k in order
    ]


def _behavior(profiles: list[dict], field: str, names: dict, order: tuple) -> list[dict]:
    groups = []
    for key in order:
        rows = [p for p in profiles if p.get(field) == key]
        if not rows:
            groups.append({"key": key, "name": names[key], "rows": []})
            continue
        n = len(rows)
        groups.append(
            {
                "key": key,
                "name": names[key],
                "rows": rows,
                "orders": sum(p["orders_30d"] for p in rows) / n,
                "kwh": sum(p["kwh_30d"] for p in rows) / n,
                "amount": sum(p["amount_30d"] for p in rows) / n,
                "active": sum(p["active_days_30d"] for p in rows) / n,
                "fast": sum(p["fast_orders"] / p["total_orders"] if p["total_orders"] else 0 for p in rows) / n,
                "focus": sum(
                    (p["station_counts"].most_common(1)[0][1] / p["total_orders"])
                    if p["total_orders"] and p["station_counts"]
                    else 0
                    for p in rows
                )
                / n,
            }
        )
    dims = ("orders", "kwh", "amount", "active", "fast", "focus")
    maxima = {d: max((g.get(d, 0) for g in groups), default=0) or 1 for d in dims}
    out = []
    for g in groups:
        if not g["rows"]:
            continue
        out.append(
            {
                "name": g["name"],
                "group": g["name"],
                "values": [round(min(100.0, g[d] / maxima[d] * 100.0), 1) for d in dims],
            }
        )
    return out


def _fmt_active(v) -> str:
    dt = _as_dt(v)
    if dt is None:
        return "--"
    return dt.strftime("%Y-%m-%d %H:%M")


def serialize_user(p: dict) -> dict:
    return {
        "user_id": p["user_id"],
        "nickname": p.get("nickname") or f"用户{p['user_id']}",
        "phone": mask_phone(p.get("phone") or ""),
        "phone_masked": mask_phone(p.get("phone") or ""),
        "frequency_level": FREQ_SHORT.get(p["frequency_level"], p["frequency_level"]),
        "value_level": VALUE_PIE.get(p["value_level"], p["value_level"]),
        "total_orders": p["total_orders"],
        "total_kwh": p["total_kwh"],
        "total_amount": p["total_amount"],
        "preferred_period": p.get("preferred_period") or "",
        "preferred_pile_type": PILE_LABEL.get(p.get("preferred_pile_type"), p.get("preferred_pile_type") or ""),
        "last_active_at": _fmt_active(p.get("last_active_at")),
    }


def list_users(profiles: list[dict], page: int, page_size: int, freq: str, value: str):
    rows = profiles
    if freq in FREQ_ORDER:
        rows = [p for p in rows if p["frequency_level"] == freq]
    if value in VALUE_ORDER:
        rows = [p for p in rows if p["value_level"] == value]
    total = len(rows)
    page = max(1, page)
    page_size = min(50, max(1, page_size))
    start = (page - 1) * page_size
    items = [serialize_user(p) for p in rows[start : start + page_size]]
    return {"items": items, "users": items, "total": total, "page": page, "page_size": page_size}


def build_trends(users: list[dict], orders: list[dict], period: str, today: date | None = None):
    today = today or date.today()
    days = {"7d": 7, "30d": 30, "90d": 90}.get(period, 30)
    start = today - timedelta(days=days - 1)
    new_by_day = Counter()
    for u in users:
        created = _as_dt(u.get("created_at"))
        if created is None:
            continue
        d = created.date()
        if start <= d <= today:
            new_by_day[d] += 1
    active_by_day = defaultdict(set)
    amount_by_day = Counter()
    for row in orders:
        end = _as_dt(row.get("end_time"))
        if end is None:
            continue
        d = end.date()
        if start <= d <= today:
            active_by_day[d].add(int(row["user_id"]))
            amount_by_day[d] += Decimal(str(row["amount"] or 0))
    items = []
    for i in range(days):
        d = start + timedelta(days=i)
        items.append(
            {
                "date": d.isoformat(),
                "day": d.isoformat(),
                "active_users": len(active_by_day.get(d, ())),
                "new_users": new_by_day.get(d, 0),
                "amount": money(amount_by_day.get(d, 0)),
            }
        )
    return items


def assemble_dashboard(users, orders, profiles, station_names, today=None, generated_at=None):
    today = today or date.today()
    generated_at = generated_at or now_str()
    period_distribution = _period_share(orders)
    insights = build_insights(profiles, period_distribution)
    user_page = list_users(profiles, 1, 10, "", "")
    return {
        "generated_at": generated_at,
        "overview": _overview(profiles, today),
        "frequency_distribution": _dist(profiles, "frequency_level", FREQ_ORDER, FREQ_PIE),
        "value_distribution": _dist(profiles, "value_level", VALUE_ORDER, VALUE_PIE),
        "segments": {
            "frequency": _dist(profiles, "frequency_level", FREQ_ORDER, FREQ_PIE),
            "value": _dist(profiles, "value_level", VALUE_ORDER, VALUE_PIE),
        },
        "attribute_distribution": {
            "age": _attr_bars(profiles, "age_group", AGE_LABEL, AGE_KEYS),
            "age_group": _attr_bars(profiles, "age_group", AGE_LABEL, AGE_KEYS),
            "city": _attr_bars(profiles, "city", {}, None),
            "source": _attr_bars(profiles, "registration_source", SOURCE_LABEL, SOURCE_KEYS),
            "registration_source": _attr_bars(
                profiles, "registration_source", SOURCE_LABEL, SOURCE_KEYS
            ),
        },
        "period_distribution": period_distribution,
        "station_preferences": _station_top(orders, station_names),
        "trends": build_trends(users, orders, "30d", today),
        "behavior_comparison": _behavior(profiles, "frequency_level", FREQ_PIE, ("high", "medium", "low")),
        "insights": [
            {
                "icon": x["icon"],
                "title": x["title"],
                "content": x["content"],
                "reason": x["reason"],
                "priority": x["priority"],
            }
            for x in insights
        ],
        "users": user_page["items"],
        "user_total": user_page["total"],
    }


def _load_bundle():
    conn = _connect()
    try:
        with conn.cursor() as cur:
            users = _fetch_users(cur)
            orders = _fetch_settled(cur)
            names = _fetch_stations(cur)
        return users, orders, names
    finally:
        conn.close()


# ---------- 路由 ----------


@bp.post("/refresh")
@require_admin
def api_refresh():
    try:
        users, orders, _names = _load_bundle()
        generated_at = now_str()
        profiles = build_profiles(users, orders)
        for p in profiles:
            p["generated_at"] = generated_at
        period_distribution = _period_share(orders)
        insights = build_insights(profiles, period_distribution)
        conn = _connect()
        try:
            with conn.cursor() as cur:
                persist_profiles(cur, profiles, generated_at)
                persist_insights(cur, insights, generated_at)
            conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            conn.close()
        return ok(
            {
                "generated_at": generated_at,
                "profile_count": len(profiles),
                "insight_count": len(insights),
            }
        )
    except pymysql.Error as exc:
        return fail(f"画像刷新失败：{exc}")


@bp.get("/dashboard")
@require_admin
def api_dashboard():
    try:
        users, orders, names = _load_bundle()
        profiles = build_profiles(users, orders)
        generated_at = now_str()
        payload = assemble_dashboard(users, orders, profiles, names, generated_at=generated_at)
        return ok(payload)
    except pymysql.Error as exc:
        return fail(f"读取群体数据失败：{exc}")


@bp.get("/users")
@require_admin
def api_users():
    try:
        users, orders, _names = _load_bundle()
        profiles = build_profiles(users, orders)
        page = int(request.args.get("page") or 1)
        page_size = int(request.args.get("page_size") or 10)
        freq = (request.args.get("frequency_level") or "").strip()
        value = (request.args.get("value_level") or "").strip()
        data = list_users(profiles, page, page_size, freq, value)
        data["generated_at"] = now_str()
        return ok(data)
    except (pymysql.Error, ValueError) as exc:
        return fail(f"读取用户明细失败：{exc}")


@bp.get("/trends")
@require_admin
def api_trends():
    try:
        users, orders, _names = _load_bundle()
        period = (request.args.get("period") or "30d").strip()
        items = build_trends(users, orders, period)
        return ok({"items": items, "trends": items, "generated_at": now_str()})
    except pymysql.Error as exc:
        return fail(f"读取趋势失败：{exc}")
