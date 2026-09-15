#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用户群体屏：口径函数 + Flask 蓝图（整包 GET / refresh）。

读临时目录里的 ADS JSON，不连 MySQL，不需要 charging-server。
"""
from __future__ import annotations

import json
import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "dashboard"))

from user_groups_api import (  # noqa: E402
    frequency_of,
    list_users,
    load_bundle,
    mask_phone,
    period_of_hour,
    preferred_pile_type,
    public_dashboard,
    reset_cache,
    value_level_from_rank,
)
from app import app  # noqa: E402


def test_helpers():
    assert frequency_of(8) == "high"
    assert frequency_of(7) == "medium"
    assert frequency_of(3) == "medium"
    assert frequency_of(2) == "low"
    assert frequency_of(1) == "low"
    assert frequency_of(0) == "inactive"

    assert [value_level_from_rank(i, 120) for i in (1, 24, 25, 84, 85, 120)] == [
        "high",
        "high",
        "medium",
        "medium",
        "low",
        "low",
    ]

    assert period_of_hour(0) == "凌晨"
    assert period_of_hour(5) == "凌晨"
    assert period_of_hour(6) == "上午"
    assert period_of_hour(11) == "上午"
    assert period_of_hour(12) == "下午"
    assert period_of_hour(17) == "下午"
    assert period_of_hour(18) == "晚间"
    assert period_of_hour(23) == "晚间"

    assert preferred_pile_type(6, 10) == "fast"
    assert preferred_pile_type(4, 10) == "slow"
    assert preferred_pile_type(5, 10) == "mixed"
    assert preferred_pile_type(0, 0) == "mixed"

    assert mask_phone("13800138001") == "138****8001"
    print("helpers ok")


def _fixture_payload():
    trends_30 = [
        {"date": f"2026-08-{d:02d}", "active_users": 1, "new_users": 0, "amount": 1.0}
        for d in range(15, 32)
    ] + [
        {"date": f"2026-09-{d:02d}", "active_users": 1, "new_users": 0, "amount": 1.0}
        for d in range(1, 14)
    ]
    dashboard = {
        "generated_at": "2026-09-13 21:00:00",
        "overview": {
            "total_users": 2,
            "active_users_30d": 1,
            "new_users_30d": 1,
            "inactive_users_30d": 1,
            "total_orders": 1,
            "total_kwh": 10.0,
            "total_amount": 12.0,
        },
        "frequency_distribution": [
            {"name": "高频用户", "value": 0, "level": "high"},
            {"name": "中频用户", "value": 0, "level": "medium"},
            {"name": "低频用户", "value": 1, "level": "low"},
            {"name": "沉默用户", "value": 1, "level": "inactive"},
        ],
        "value_distribution": [
            {"name": "高价值", "value": 0, "level": "high"},
            {"name": "中价值", "value": 1, "level": "medium"},
            {"name": "低价值", "value": 1, "level": "low"},
        ],
        "attribute_distribution": {
            "age": [{"name": "25岁以下", "value": 1}],
            "city": [{"name": "深圳", "value": 2}],
            "source": [{"name": "Android", "value": 2}],
        },
        "period_distribution": [
            {"name": "凌晨", "value": 0.0},
            {"name": "上午", "value": 0.0},
            {"name": "下午", "value": 0.0},
            {"name": "晚间", "value": 100.0},
        ],
        "station_preferences": [{"name": "市民中心站", "user_count": 1}],
        "trends": trends_30[-30:],
        "trends_7d": trends_30[-7:],
        "trends_90d": trends_30,
        "behavior_comparison": [{"name": "低频用户", "values": [18.3, 19.1, 18.6, 20.8, 100.0, 100.0]}],
        "insights": [
            {
                "icon": "⚠",
                "title": "建议开展沉默用户唤醒",
                "content": "可针对近30天未充电用户发放限时优惠券。",
                "reason": "沉默用户占比50.0%",
                "priority": 1,
            }
        ],
        "users": [],
        "user_total": 2,
    }
    users = [
        {
            "user_id": 1,
            "nickname": "演示",
            "phone_masked": "138****8001",
            "frequency_level": "低频",
            "frequency_key": "low",
            "value_level": "中价值",
            "value_key": "medium",
            "total_orders": 1,
            "total_kwh": 10.0,
            "total_amount": 12.0,
            "preferred_period": "晚间",
            "preferred_pile_type": "快充",
            "last_active_at": "2026-09-14 12:00:00",
        },
        {
            "user_id": 2,
            "nickname": "沉默",
            "phone_masked": "137****0001",
            "frequency_level": "沉默",
            "frequency_key": "inactive",
            "value_level": "低价值",
            "value_key": "low",
            "total_orders": 0,
            "total_kwh": 0.0,
            "total_amount": 0.0,
            "preferred_period": "--",
            "preferred_pile_type": "混合",
            "last_active_at": "--",
        },
    ]
    dashboard["users"] = users[:10]
    return dashboard, users


def write_fixture(tmpdir: Path) -> None:
    dashboard, users = _fixture_payload()
    (tmpdir / "dashboard.json").write_text(
        json.dumps(dashboard, ensure_ascii=False), encoding="utf-8"
    )
    (tmpdir / "users.json").write_text(json.dumps(users, ensure_ascii=False), encoding="utf-8")


def test_json_bundle_and_dashboard():
    dashboard, users = _fixture_payload()
    payload = public_dashboard(dashboard, users)
    assert payload["overview"]["total_users"] == 2
    assert payload["overview"]["inactive_users_30d"] == 1
    assert payload["generated_at"] == "2026-09-13 21:00:00"
    assert payload["frequency_distribution"][2]["level"] == "low"
    assert payload["users"][0]["phone_masked"] == "138****8001"
    assert payload["station_preferences"][0]["station_name"] == "市民中心站"
    page = list_users(users, 1, 10, "low", "")
    assert page["total"] == 1
    assert page["items"][0]["nickname"] == "演示"
    empty = list_users(users, 1, 10, "high", "")
    assert empty["total"] == 0
    print("json dashboard ok")


def test_flask_routes():
    client = app.test_client()
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        write_fixture(tmpdir)
        reset_cache()
        os.environ["USER_GROUPS_DATA_DIR"] = str(tmpdir)
        try:
            dash = client.get("/api/admin/user-groups/dashboard")
            assert dash.status_code == 200, dash.get_data(as_text=True)
            wrapped = dash.get_json()
            assert wrapped["code"] == 0
            data = wrapped["data"]
            for key in (
                "overview",
                "frequency_distribution",
                "value_distribution",
                "attribute_distribution",
                "period_distribution",
                "trends",
                "insights",
                "users",
                "generated_at",
            ):
                assert key in data, key
            ov = data["overview"]
            assert ov["total_users"] == 2
            assert len(data["frequency_distribution"]) == 4
            assert len(data["value_distribution"]) == 3
            assert data["generated_at"] == "2026-09-13 21:00:00"

            users = client.get("/api/admin/user-groups/users?page=1&page_size=10")
            assert users.status_code == 200, users.get_data(as_text=True)
            assert users.get_json()["data"]["total"] == ov["total_users"]

            filtered = client.get("/api/admin/user-groups/users?frequency_level=low")
            assert filtered.get_json()["data"]["total"] == 1

            trends = client.get("/api/admin/user-groups/trends?period=7d")
            assert trends.status_code == 200, trends.get_data(as_text=True)
            assert len(trends.get_json()["data"]["items"]) == 7

            refresh = client.post("/api/admin/user-groups/refresh")
            assert refresh.status_code == 200, refresh.get_data(as_text=True)
            rdata = refresh.get_json()["data"]
            assert rdata["profile_count"] == ov["total_users"]
            assert rdata["generated_at"] == "2026-09-13 21:00:00"
        finally:
            os.environ.pop("USER_GROUPS_DATA_DIR", None)
            reset_cache()
    print("flask routes ok")


def test_real_ads_snapshot_if_present():
    ads = ROOT / "bigdata" / "work" / "ads" / "user_groups"
    if not (ads / "dashboard.json").is_file() or not (ads / "users.json").is_file():
        print("real ads snapshot skipped")
        return
    reset_cache()
    os.environ.pop("USER_GROUPS_DATA_DIR", None)
    dashboard, users = load_bundle(force=True)
    payload = public_dashboard(dashboard, users)
    assert payload["overview"]["total_users"] == 11247
    assert payload["overview"]["inactive_users_30d"] == 3037
    assert [x["value"] for x in payload["frequency_distribution"]] == [1349, 3149, 3712, 3037]
    assert payload["generated_at"] == "2026-09-13 21:00:00"
    assert payload["users"][0]["frequency_level"] in ("高频", "中频", "低频", "沉默")
    reset_cache()
    print("real ads snapshot ok")


def main():
    test_helpers()
    test_json_bundle_and_dashboard()
    test_flask_routes()
    test_real_ads_snapshot_if_present()
    print("user_groups_api ok")


if __name__ == "__main__":
    main()
