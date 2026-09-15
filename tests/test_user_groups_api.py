#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用户群体屏：口径函数 + Flask 蓝图（401 / 整包 GET / refresh）。

不需要 charging-server：鉴权在用例里 mock。需要本机 MySQL charging_system。
"""
from __future__ import annotations

import sys
from datetime import date, datetime
from decimal import Decimal
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "dashboard"))

from user_groups_api import (  # noqa: E402
    assemble_dashboard,
    build_profiles,
    frequency_of,
    mask_phone,
    period_of_hour,
    preferred_pile_type,
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


def test_build_profiles_and_dashboard():
    today = date(2026, 9, 15)
    users = [
        {
            "id": 1,
            "phone": "13800138001",
            "nickname": "演示",
            "age_group": "25_34",
            "city": "深圳",
            "registration_source": "qt",
            "created_at": datetime(2026, 8, 1, 10, 0, 0),
            "last_active_at": datetime(2026, 9, 14, 12, 0, 0),
        },
        {
            "id": 2,
            "phone": "13700000001",
            "nickname": "沉默",
            "age_group": "under_25",
            "city": "",
            "registration_source": "android",
            "created_at": datetime(2026, 9, 1, 9, 0, 0),
            "last_active_at": None,
        },
    ]
    orders = [
        {
            "user_id": 1,
            "station_id": 1,
            "kwh": Decimal("10.00"),
            "amount": Decimal("12.00"),
            "duration_seconds": 1800,
            "start_time": datetime(2026, 9, 10, 20, 0, 0),
            "end_time": datetime(2026, 9, 10, 20, 30, 0),
            "created_at": datetime(2026, 9, 10, 19, 50, 0),
            "pile_type": "fast",
        }
    ]
    profiles = build_profiles(users, orders, today)
    assert len(profiles) == 2
    by_id = {p["user_id"]: p for p in profiles}
    assert by_id[1]["frequency_level"] == "low"
    assert by_id[1]["preferred_period"] == "晚间"
    assert by_id[1]["preferred_pile_type"] == "fast"
    assert by_id[2]["frequency_level"] == "inactive"
    assert by_id[1]["value_level"] == "medium"  # n=2: high_end=0, medium_end=1
    assert by_id[2]["value_level"] == "low"

    payload = assemble_dashboard(
        users, orders, profiles, {1: "市民中心站"}, today, generated_at="2026-09-15 09:00:00"
    )
    assert payload["overview"]["total_users"] == 2
    assert payload["overview"]["inactive_users_30d"] == 1
    assert payload["overview"]["total_kwh"] == 10.0
    assert payload["frequency_distribution"][0]["level"] == "high"
    assert any(x["name"] == "晚间" and x["value"] == 100.0 for x in payload["period_distribution"])
    assert payload["station_preferences"][0]["name"] == "市民中心站"
    assert payload["generated_at"] == "2026-09-15 09:00:00"
    print("profiles/dashboard ok")


def test_flask_auth_and_routes():
    client = app.test_client()
    missing = client.get("/api/admin/user-groups/dashboard")
    assert missing.status_code == 401, missing.get_json()
    body = missing.get_json()
    assert body["code"] == 9

    with patch("user_groups_api.validate_admin_token", return_value=(True, 200, "")):
        dash = client.get(
            "/api/admin/user-groups/dashboard",
            headers={"Authorization": "Bearer test-token"},
        )
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
        assert ov["total_users"] >= 0
        assert isinstance(data["frequency_distribution"], list)
        assert len(data["frequency_distribution"]) == 4
        assert len(data["value_distribution"]) == 3

        users = client.get(
            "/api/admin/user-groups/users?page=1&page_size=10",
            headers={"Authorization": "Bearer test-token"},
        )
        assert users.status_code == 200, users.get_data(as_text=True)
        assert users.get_json()["data"]["total"] == ov["total_users"]

        trends = client.get(
            "/api/admin/user-groups/trends?period=7d",
            headers={"Authorization": "Bearer test-token"},
        )
        assert trends.status_code == 200, trends.get_data(as_text=True)
        assert len(trends.get_json()["data"]["items"]) == 7

        refresh = client.post(
            "/api/admin/user-groups/refresh",
            headers={"Authorization": "Bearer test-token"},
        )
        assert refresh.status_code == 200, refresh.get_data(as_text=True)
        rdata = refresh.get_json()["data"]
        assert rdata["profile_count"] == ov["total_users"]
        assert "generated_at" in rdata
    print("flask routes ok")


def main():
    test_helpers()
    test_build_profiles_and_dashboard()
    test_flask_auth_and_routes()
    print("user_groups_api ok")


if __name__ == "__main__":
    main()
