#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用户群体运营大屏 API（/user-groups）。

读 Spark 产出的 ADS JSON（dashboard.json + users.json）。
和运营屏一样只读快照，不连 MySQL，也不验管理员 token。
"""
from __future__ import annotations

import json
import os
import threading
from datetime import datetime
from pathlib import Path

from flask import Blueprint, jsonify, request

bp = Blueprint("user_groups", __name__, url_prefix="/api/admin/user-groups")

ROOT = Path(__file__).resolve().parent
DEFAULT_DATA_DIR = ROOT.parent / "bigdata" / "work" / "ads" / "user_groups"

FREQ_FILTER = {
    "high": "high",
    "高频": "high",
    "高频用户": "high",
    "medium": "medium",
    "中频": "medium",
    "中频用户": "medium",
    "low": "low",
    "低频": "low",
    "低频用户": "low",
    "inactive": "inactive",
    "沉默": "inactive",
    "沉默用户": "inactive",
}
VALUE_FILTER = {
    "high": "high",
    "高价值": "high",
    "medium": "medium",
    "中价值": "medium",
    "low": "low",
    "低价值": "low",
}
TREND_KEYS = {"7d": "trends_7d", "30d": "trends", "90d": "trends_90d"}

_CACHE_LOCK = threading.Lock()
_CACHE: dict = {"mtime": None, "dashboard": None, "users": None}


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


def now_str(now: datetime | None = None) -> str:
    return (now or datetime.now()).strftime("%Y-%m-%d %H:%M:%S")


def ok(data):
    return jsonify({"code": 0, "data": data})


def fail(msg: str, status: int = 500, code: int = 1):
    return jsonify({"code": code, "msg": msg}), status


# ---------- ADS JSON ----------


def data_dir() -> Path:
    env = os.environ.get("USER_GROUPS_DATA_DIR")
    if env:
        return Path(env)
    return DEFAULT_DATA_DIR


def reset_cache() -> None:
    with _CACHE_LOCK:
        _CACHE["mtime"] = None
        _CACHE["dashboard"] = None
        _CACHE["users"] = None


def _read_json(path: Path):
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_bundle(force: bool = False) -> tuple[dict, list]:
    ddir = data_dir()
    dash_path = ddir / "dashboard.json"
    users_path = ddir / "users.json"
    if not dash_path.is_file() or not users_path.is_file():
        raise FileNotFoundError(
            "未找到群体 ADS 快照，请先 spark-submit bigdata/user_groups_pipeline.py"
        )
    mtime = max(dash_path.stat().st_mtime, users_path.stat().st_mtime)
    with _CACHE_LOCK:
        if (
            not force
            and _CACHE["dashboard"] is not None
            and _CACHE["users"] is not None
            and _CACHE["mtime"] == mtime
        ):
            return _CACHE["dashboard"], _CACHE["users"]
        dashboard = _read_json(dash_path)
        users = _read_json(users_path)
        if not isinstance(dashboard, dict):
            raise ValueError("dashboard.json 格式错误")
        if not isinstance(users, list):
            raise ValueError("users.json 格式错误")
        _CACHE["mtime"] = mtime
        _CACHE["dashboard"] = dashboard
        _CACHE["users"] = users
        return dashboard, users


def serialize_user(p: dict) -> dict:
    phone = p.get("phone_masked") or mask_phone(p.get("phone") or "")
    return {
        "user_id": p.get("user_id"),
        "nickname": p.get("nickname") or f"用户{p.get('user_id')}",
        "phone": phone,
        "phone_masked": phone,
        "frequency_level": p.get("frequency_level") or "",
        "value_level": p.get("value_level") or "",
        "frequency_key": p.get("frequency_key") or "",
        "value_key": p.get("value_key") or "",
        "total_orders": p.get("total_orders") or 0,
        "total_kwh": p.get("total_kwh") or 0,
        "total_amount": p.get("total_amount") or 0,
        "preferred_period": p.get("preferred_period") or "",
        "preferred_pile_type": p.get("preferred_pile_type") or "",
        "last_active_at": p.get("last_active_at") or "--",
    }


def list_users(profiles: list[dict], page: int, page_size: int, freq: str, value: str):
    rows = profiles
    freq_key = FREQ_FILTER.get(freq.strip(), "") if freq else ""
    value_key = VALUE_FILTER.get(value.strip(), "") if value else ""
    if freq_key:
        rows = [p for p in rows if (p.get("frequency_key") or "") == freq_key]
    if value_key:
        rows = [p for p in rows if (p.get("value_key") or "") == value_key]
    total = len(rows)
    page = max(1, page)
    page_size = min(50, max(1, page_size))
    start = (page - 1) * page_size
    items = [serialize_user(p) for p in rows[start : start + page_size]]
    return {"items": items, "users": items, "total": total, "page": page, "page_size": page_size}


def _trend_items(dashboard: dict, period: str) -> list:
    key = TREND_KEYS.get(period, "trends")
    items = dashboard.get(key)
    if isinstance(items, list) and items:
        return items
    trends = dashboard.get("trends") or []
    days = {"7d": 7, "30d": 30, "90d": 90}.get(period, 30)
    return trends[-days:]


def public_dashboard(dashboard: dict, users: list[dict]) -> dict:
    payload = dict(dashboard)
    freq = payload.get("frequency_distribution") or []
    val = payload.get("value_distribution") or []
    payload.setdefault("segments", {"frequency": freq, "value": val})
    attrs = dict(payload.get("attribute_distribution") or {})
    if "age" in attrs:
        attrs.setdefault("age_group", attrs["age"])
    if "source" in attrs:
        attrs.setdefault("registration_source", attrs["source"])
    payload["attribute_distribution"] = attrs
    stations = []
    for row in payload.get("station_preferences") or []:
        name = row.get("name") or row.get("station_name") or ""
        count = row.get("user_count") if row.get("user_count") is not None else row.get("count") or 0
        stations.append(
            {
                **row,
                "name": name,
                "station_name": name,
                "user_count": count,
                "count": count,
            }
        )
    payload["station_preferences"] = stations
    page = list_users(users, 1, 10, "", "")
    payload["users"] = page["items"]
    payload["user_total"] = payload.get("user_total") or page["total"]
    payload["generated_at"] = payload.get("generated_at") or now_str()
    return payload


# ---------- 路由 ----------


@bp.post("/refresh")
def api_refresh():
    try:
        dashboard, users = load_bundle(force=True)
        payload = public_dashboard(dashboard, users)
        return ok(
            {
                "generated_at": payload["generated_at"],
                "profile_count": payload["user_total"],
                "insight_count": len(payload.get("insights") or []),
            }
        )
    except FileNotFoundError as exc:
        return fail(str(exc), status=503)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return fail(f"画像刷新失败：{exc}")


@bp.get("/dashboard")
def api_dashboard():
    try:
        dashboard, users = load_bundle()
        return ok(public_dashboard(dashboard, users))
    except FileNotFoundError as exc:
        return fail(str(exc), status=503)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return fail(f"读取群体数据失败：{exc}")


@bp.get("/users")
def api_users():
    try:
        dashboard, users = load_bundle()
        page = int(request.args.get("page") or 1)
        page_size = int(request.args.get("page_size") or 10)
        freq = (request.args.get("frequency_level") or "").strip()
        value = (request.args.get("value_level") or "").strip()
        data = list_users(users, page, page_size, freq, value)
        data["generated_at"] = dashboard.get("generated_at") or now_str()
        return ok(data)
    except FileNotFoundError as exc:
        return fail(str(exc), status=503)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return fail(f"读取用户明细失败：{exc}")


@bp.get("/trends")
def api_trends():
    try:
        dashboard, _users = load_bundle()
        period = (request.args.get("period") or "30d").strip()
        items = _trend_items(dashboard, period)
        return ok(
            {
                "items": items,
                "trends": items,
                "generated_at": dashboard.get("generated_at") or now_str(),
            }
        )
    except FileNotFoundError as exc:
        return fail(str(exc), status=503)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return fail(f"读取趋势失败：{exc}")
