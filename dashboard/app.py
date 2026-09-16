#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第二阶段运营大屏 Flask API（NO.119 / 老师（5））。

端口 8081，不直连演示库 MySQL。仓库里若有 ADS 快照
bigdata/work/ads/kpis/dashboard.json，直接 python3 dashboard/app.py 就会读它；
没有快照才回落到内置 mock。仍可用 DASHBOARD_DATA_FILE 覆盖路径。
本机若有 Spark 探查报告，质量区叠 bigdata/work/qa/report.json。

矩阵约定：
  GET /api/quality  /api/kpis  /api/load  /api/stations  /api/alerts
  GET /api/dashboard  整包（大屏一次拉取）
"""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path

from flask import Flask, jsonify, request, send_from_directory

ROOT = Path(__file__).resolve().parent
FRONTEND = ROOT / "frontend"
ADS_DASHBOARD = ROOT.parent / "bigdata" / "work" / "ads" / "kpis" / "dashboard.json"
DATA_FILE_ENV = os.environ.get("DASHBOARD_DATA_FILE")
if DATA_FILE_ENV:
    DATA_FILE = Path(DATA_FILE_ENV)
elif ADS_DASHBOARD.is_file():
    DATA_FILE = ADS_DASHBOARD
else:
    DATA_FILE = ROOT / "mock_data.json"
USING_ADS_FILE = bool(DATA_FILE_ENV) or (
    DATA_FILE.is_file() and DATA_FILE.resolve() == ADS_DASHBOARD.resolve()
)
QA_FILE = Path(
    os.environ.get(
        "DASHBOARD_QA_FILE",
        ROOT.parent / "bigdata" / "work" / "qa" / "report.json",
    )
)

app = Flask(__name__, static_folder=str(FRONTEND), static_url_path="")

# 用户群体屏：读 ADS JSON（bigdata/work/ads/user_groups/），不连 MySQL。
# 运营首页 /api/dashboard 仍只读 kpis/dashboard.json。
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from user_groups_api import bp as user_groups_bp  # noqa: E402

app.register_blueprint(user_groups_bp)

QUALITY_KEYS = (
    "ods_rows",
    "null_start_time",
    "negative_kwh",
    "dup_order_no",
    "orphan_station",
    "dwd_rows",
)


def mock_data() -> dict:
    cap, warn = 2400.0, 1920.0

    def stamp(points):
        if points is None:
            return None
        return [
            {**p, "capacity_kw": cap, "warning_threshold_kw": warn} for p in points
        ]

    load_today = stamp(
        [{"hour": h, "kwh": round(18 + (h % 12) * 4.2, 1)} for h in range(24)]
    )
    load_today[8]["kwh"] = 96.0
    load_today[9]["kwh"] = 102.5
    load_today[18]["kwh"] = 1280.5 / 10
    yday = stamp([{"hour": h, "kwh": round(16 + (h % 12) * 3.8, 1)} for h in range(24)])
    return {
        "generated_at": "2026-09-13T21:00:00",
        "kpis": {
            "order_count": 16924,
            "total_kwh": 609996.23,
            "total_revenue": 787325.59,
            "active_stations": 72,
            "today_kwh": 1280.5,
            "today_revenue": 1920.75,
            "idle_piles": 210,
            "busy_piles": 160,
            "fault_piles": 29,
            "peak_hour": "18:00",
            "alert_count": 3,
            "yesterday_kwh": 1190.2,
            "yesterday_charge_kwh": 1190.2,
            "yesterday_revenue": 1785.40,
        },
        "quality": {
            "ods_rows": 20000,
            "null_start_time": 2014,
            "negative_kwh": 177,
            "dup_order_no": 177,
            "orphan_station": 177,
            "dwd_rows": 16924,
        },
        "load_today": load_today,
        "load_forecast_24h": [
            {"offset": i, "kwh": round(48 + 3.5 * ((i - 1) % 12), 1)}
            for i in range(1, 25)
        ],
        # ADS dashboard aggregates.  These are read-only presentation fields
        # exported by the warehouse job and intentionally kept alongside the
        # legacy chart series for backwards compatibility.
        "load_hour_avg": [{"hour": h, "kwh": round(40 + (h % 12) * 8.5, 2)} for h in range(24)],
        "charge_heatmap": {"x_axis": list(range(0, 24, 2)), "y_axis": ["周一", "周二", "周三", "周四", "周五", "周六", "周日"], "series_data": [[x, y, round(20 + ((x + y * 3) % 12) * 6.5, 2)] for y in range(7) for x in range(12)], "min_val": 0, "max_val": 100},
        # No reliable source/defined business formula exists for the optional
        # turnover and per-pile summary metrics; keep the payload empty rather
        # than publishing fabricated demo values.
        "summary": {},
        "station_rank": [], "overstay_records": [], "device_warnings": [], "active_tickets": [],
        "weekday_weekend": {
            "weekday_kwh": 423647.05,
            "weekend_kwh": 180063.34,
            "note": "DWS 分摊，周一至周五 vs 周六日",
        },
        "regions": [
            {"city": city, "kwh": kwh, "amount": amount, "yuan_per_kwh": round(amount / kwh, 4)}
            for city, kwh, amount in (
                ("北京", 150000.0, 205000.0), ("广州", 130000.0, 175000.0),
                ("杭州", 120000.0, 160000.0), ("上海", 110000.0, 150000.0),
                ("深圳", 90000.0, 125000.0), ("南京", 70000.0, 98000.0),
            )
        ],
        "pile_types": [
            {"type": "直流", "idle": 220, "busy": 41, "fault": 27, "util": 15.7},
            {"type": "交流", "idle": 82, "busy": 20, "fault": 9, "util": 19.6},
        ],
        "stations": [
            {
                "station_id": 1,
                "name": "深圳市民中心充电站",
                "idle": 2,
                "total": 4,
                "latitude": 22.5431,
                "longitude": 114.0579,
                "capacity_kw": 240.0,
                "warning_threshold_kw": 192.0,
                "forecast": {
                    "h1": {"kwh": 8.2, "idle": 2, "util": 50.0, "is_peak": 0, "congestion": "mid"},
                    "h6": {"kwh": 22.0, "idle": 1, "util": 83.3, "is_peak": 1, "congestion": "high"},
                    "h24": {"kwh": 18.0, "idle": 2, "util": 66.7, "is_peak": 0, "congestion": "mid"},
                },
            },
            {
                "station_id": 8,
                "name": "上海陆家嘴充电站",
                "idle": 1,
                "total": 4,
                "latitude": 31.2304,
                "longitude": 121.4737,
                "capacity_kw": 240.0,
                "warning_threshold_kw": 192.0,
                "forecast": {
                    "h1": {"kwh": 6.0, "idle": 1, "util": 67.0, "is_peak": 0, "congestion": "mid"},
                    "h6": {"kwh": 14.0, "idle": 1, "util": 75.0, "is_peak": 0, "congestion": "mid"},
                    "h24": {"kwh": 11.0, "idle": 2, "util": 50.0, "is_peak": 0, "congestion": "low"},
                },
            },
            {
                "station_id": 12,
                "name": "广州天河充电站",
                "idle": 3,
                "total": 5,
                "latitude": 23.1291,
                "longitude": 113.2644,
                "capacity_kw": 300.0,
                "warning_threshold_kw": 240.0,
                "forecast": {
                    "h1": {"kwh": 5.1, "idle": 3, "util": 40.0, "is_peak": 0, "congestion": "low"},
                    "h6": {"kwh": 9.4, "idle": 2, "util": 52.0, "is_peak": 0, "congestion": "mid"},
                    "h24": {"kwh": 8.0, "idle": 3, "util": 40.0, "is_peak": 0, "congestion": "low"},
                },
            },
            {
                "station_id": 31,
                "name": "北京南站充电站",
                "idle": 0,
                "total": 4,
                "latitude": 39.8650,
                "longitude": 116.3785,
                "capacity_kw": 240.0,
                "warning_threshold_kw": 192.0,
                "forecast": {
                    "h1": {"kwh": 12.0, "idle": 0, "util": 88.0, "is_peak": 1, "congestion": "high"},
                    "h6": {"kwh": 16.5, "idle": 0, "util": 92.0, "is_peak": 1, "congestion": "high"},
                    "h24": {"kwh": 10.2, "idle": 1, "util": 75.0, "is_peak": 0, "congestion": "mid"},
                },
            },
        ],
        "alerts": [
            {
                "alert_id": "31-h1",
                "station_id": 31,
                "name": "北京南站充电站",
                "station_name": "北京南站充电站",
                "horizon": 1,
                "reason": "1小时后预测占用率 88%",
                "message": "1小时后预测占用率 88%",
                "alert_type": "peak",
                "severity": "high",
                "predicted_occupancy": 88.0,
                "predicted_time": "2026-09-13T22:00:00",
                "created_at": "2026-09-13T21:00:00",
            },
            {
                "alert_id": "1-h6",
                "station_id": 1,
                "name": "深圳市民中心充电站",
                "station_name": "深圳市民中心充电站",
                "horizon": 6,
                "reason": "6小时后预测占用率 83%",
                "message": "6小时后预测占用率 83%",
                "alert_type": "peak",
                "severity": "high",
                "predicted_occupancy": 83.0,
                "predicted_time": "2026-09-14T03:00:00",
                "created_at": "2026-09-13T21:00:00",
            },
            {
                "alert_id": "31-h6",
                "station_id": 31,
                "name": "北京南站充电站",
                "station_name": "北京南站充电站",
                "horizon": 6,
                "reason": "6小时后预测占用率 92%",
                "message": "6小时后预测占用率 92%",
                "alert_type": "peak",
                "severity": "high",
                "predicted_occupancy": 92.0,
                "predicted_time": "2026-09-14T03:00:00",
                "created_at": "2026-09-13T21:00:00",
            },
        ],
        "faults": [
            {
                "pile_id": 4,
                "code": "BJ001-01",
                "station_id": 4,
                "station_name": "北京国贸充电站",
                "fault_type": "fault",
                "fault_code": None,
                "fault_time": "2026-09-13T18:40:00",
                "status": "fault",
            }
        ],
        "dispatch": [
            {
                "source_station_id": 31,
                "source_station_name": "北京南站充电站",
                "source_predicted_occupancy": 88.0,
                "recommended_station_id": 1,
                "recommended_station_name": "深圳市民中心充电站",
                "recommended_idle_piles": 2,
                "recommended_occupancy": 50.0,
                "distance_km": 1932.4,
                "expected_improvement": 38.0,
                "reason": "预测占用 88%，引导至空闲站（空闲 2 桩）",
                "created_at": "2026-09-13T21:00:00",
                "from_name": "北京南站充电站",
                "to_name": "深圳市民中心充电站",
                "from_id": 31,
                "to_id": 1,
            }
        ],
        "targets": {
            "date": "2026-09-13",
            "charge_kwh_target": 1190.2,
            "revenue_target": 1785.40,
            "availability_target": 90.0,
        },
        "yesterday_load": yday,
        "windows": {
            "1": {
                "start": "2026-09-13",
                "end": "2026-09-13",
                "kpis": {
                    "today_kwh": 1280.5,
                    "today_revenue": 1920.75,
                    "yesterday_kwh": 1190.2,
                    "yesterday_charge_kwh": 1190.2,
                    "yesterday_revenue": 1785.40,
                    "peak_hour": "18:00",
                },
                "load_today": load_today,
                "yesterday_load": yday,
                "targets": {
                    "date": "2026-09-13",
                    "charge_kwh_target": 1190.2,
                    "revenue_target": 1785.40,
                    "availability_target": 90.0,
                },
            },
            "7": {
                "start": "2026-09-07",
                "end": "2026-09-13",
                "kpis": {
                    "today_kwh": 8120.4,
                    "today_revenue": 12180.6,
                    "yesterday_kwh": 7640.1,
                    "yesterday_charge_kwh": 7640.1,
                    "yesterday_revenue": 11460.2,
                    "peak_hour": "18:00",
                },
                "load_today": stamp(
                    [{"hour": h, "kwh": round(40 + (h % 12) * 8.5, 1)} for h in range(24)]
                ),
                "yesterday_load": stamp(
                    [{"hour": h, "kwh": round(36 + (h % 12) * 8.0, 1)} for h in range(24)]
                ),
                "targets": {
                    "date": "2026-09-13",
                    "charge_kwh_target": 7640.1,
                    "revenue_target": 11460.2,
                    "availability_target": 90.0,
                },
            },
            "30": {
                "start": "2026-08-15",
                "end": "2026-09-13",
                "kpis": {
                    "today_kwh": 15620.04,
                    "today_revenue": 20292.01,
                    "yesterday_kwh": None,
                    "yesterday_charge_kwh": None,
                    "yesterday_revenue": None,
                    "peak_hour": "11:00",
                },
                "load_today": stamp(
                    [{"hour": h, "kwh": round(50 + (h % 12) * 10.2, 1)} for h in range(24)]
                ),
                "yesterday_load": None,
                "targets": {
                    "date": "2026-09-13",
                    "charge_kwh_target": None,
                    "revenue_target": None,
                    "availability_target": 90.0,
                },
            },
        },
        "latest_data_time": "2026-09-13T21:00:00",
        "data_source": "mock",
        "freshness_status": "mock",
    }


def overlay_quality(data: dict) -> None:
    """有 qa/report.json 时，质量区换成探查真值（键名对齐矩阵）。"""
    if not QA_FILE.is_file():
        return
    try:
        qa = json.loads(QA_FILE.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return
    if not isinstance(qa, dict):
        return
    quality = dict(data.get("quality") or {})
    for key in QUALITY_KEYS:
        if key in qa:
            quality[key] = qa[key]
    if "dup_order_no" not in qa and "duplicate_order_no" in qa:
        quality["dup_order_no"] = qa["duplicate_order_no"]
    data["quality"] = quality
    if qa.get("generated_at") and not data.get("generated_at"):
        data["generated_at"] = qa["generated_at"]


def parse_period() -> str:
    raw = (request.args.get("period") or "1").strip().lower()
    aliases = {"today": "1", "1d": "1", "7d": "7", "30d": "30"}
    raw = aliases.get(raw, raw)
    return raw if raw in ("1", "7", "30") else "1"


def apply_period(data: dict, period: str) -> dict:
    data = dict(data or {})
    windows = data.get("windows") or {}
    win = windows.get(period) or {}
    data["period"] = period
    if not win:
        return data
    kpis = dict(data.get("kpis") or {})
    for key, value in (win.get("kpis") or {}).items():
        kpis[key] = value
    if "yesterday_charge_kwh" not in kpis and "yesterday_kwh" in kpis:
        kpis["yesterday_charge_kwh"] = kpis["yesterday_kwh"]
    data["kpis"] = kpis
    if win.get("load_today") is not None:
        data["load_today"] = win["load_today"]
    data["yesterday_load"] = win.get("yesterday_load")
    if win.get("targets") is not None:
        data["targets"] = win["targets"]
    data["window_start"] = win.get("start")
    data["window_end"] = win.get("end")
    return data


def load_snapshot() -> dict:
    # A configured ADS snapshot is authoritative: never fill missing ADS fields
    # with demo values, otherwise the screen can mix real and fake metrics.
    try:
        if DATA_FILE.is_file():
            snapshot = json.loads(DATA_FILE.read_text(encoding="utf-8"))
            if isinstance(snapshot, dict) and snapshot:
                return snapshot
            if DATA_FILE_ENV:
                raise ValueError("ADS dashboard JSON must be a non-empty object")
        elif DATA_FILE_ENV:
            raise FileNotFoundError(f"ADS dashboard JSON not found: {DATA_FILE}")
    except (OSError, ValueError):
        if DATA_FILE_ENV:
            raise

    data = mock_data()
    overlay_quality(data)
    kpis = data.setdefault("kpis", {})
    kpis["alert_count"] = len(data.get("alerts") or [])
    return data


def normalize_snapshot(data: dict) -> dict:
    """Fill only deterministic presentation fields from an ADS snapshot.

    ADS exports from different pipeline runs use slightly different names;
    normalising here keeps the page useful without inventing business data.
    """
    data = dict(data or {})
    kpis = dict(data.get("kpis") or {})
    aliases = {"total_kwh": ("today_kwh", "total_kwh"),
               "total_revenue": ("today_revenue", "total_revenue"),
               "active_stations": ("active_stations", "station_count")}
    for target, names in aliases.items():
        if target not in kpis:
            for name in names:
                if name in kpis:
                    kpis[target] = kpis[name]
                    break
    stations = list(data.get("stations") or [])
    if stations and not any(k in kpis for k in ("idle_piles", "busy_piles", "fault_piles")):
        kpis["idle_piles"] = sum(int(s.get("idle") or 0) for s in stations)
        kpis["busy_piles"] = sum(max(0, int(s.get("total") or 0) - int(s.get("idle") or 0)) for s in stations)
        kpis["fault_piles"] = sum(int(s.get("fault") or 0) for s in stations)
    if "active_stations" not in kpis:
        kpis["active_stations"] = sum(1 for s in stations if int(s.get("total") or 0) > 0)
    kpis["alert_count"] = len(data.get("alerts") or [])
    data["kpis"] = kpis
    data.setdefault("dispatch", [])
    data.setdefault("targets", {})
    data.setdefault("faults", [])
    data.setdefault("load_today", [])
    data.setdefault("load_forecast_24h", [])
    data.setdefault("load_hour_avg", [])
    data.setdefault("charge_heatmap", {})
    data.setdefault("summary", {})
    data.setdefault("station_rank", [])
    data.setdefault("overstay_records", [])
    data.setdefault("device_warnings", [])
    data.setdefault("active_tickets", [])
    data.setdefault("weekday_weekend", {})
    data.setdefault("regions", [])
    data.setdefault("pile_types", [])
    data.setdefault("stations", stations)
    data.setdefault("yesterday_load", None)
    data.setdefault("windows", {})
    data.setdefault("latest_data_time", data.get("generated_at"))
    data.setdefault("data_source", "ads" if USING_ADS_FILE else "mock")
    data.setdefault("freshness_status", data.get("data_source"))
    return data


@app.get("/api/dashboard")
def api_dashboard():
    period = parse_period()
    return jsonify(apply_period(normalize_snapshot(load_snapshot()), period))


@app.get("/api/quality")
def api_quality():
    return jsonify(load_snapshot().get("quality", {}))


@app.get("/api/kpis")
def api_kpis():
    period = parse_period()
    data = apply_period(normalize_snapshot(load_snapshot()), period)
    return jsonify(data.get("kpis", {}))


@app.get("/api/load")
def api_load():
    period = parse_period()
    snap = apply_period(normalize_snapshot(load_snapshot()), period)
    return jsonify(
        {
            "load_today": snap.get("load_today", []),
            "load_forecast_24h": snap.get("load_forecast_24h", []),
            "yesterday_load": snap.get("yesterday_load"),
            "period": snap.get("period"),
        }
    )


@app.get("/api/stations")
def api_stations():
    period = parse_period()
    snap = apply_period(normalize_snapshot(load_snapshot()), period)
    return jsonify({"stations": snap.get("stations", []), "period": snap.get("period")})


@app.get("/api/alerts")
def api_alerts():
    period = parse_period()
    snap = apply_period(normalize_snapshot(load_snapshot()), period)
    return jsonify({"alerts": snap.get("alerts", []), "period": snap.get("period")})


@app.get("/")
def index():
    return send_from_directory(FRONTEND, "index.html")


@app.get("/user-groups")
def user_groups():
    return send_from_directory(FRONTEND, "user-groups.html")


if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=int(os.environ.get("DASHBOARD_PORT", "8081")),
        debug=False,
    )
