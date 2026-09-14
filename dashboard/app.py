#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第二阶段运营大屏 Flask API（NO.119 / 老师（5））。

端口 8081，不直连演示库 MySQL。默认读 dashboard/mock_data.json；
本机若有 Spark 探查报告，质量区叠 bigdata/work/qa/report.json。
接真 ADS 时设 DASHBOARD_DATA_FILE 指向流水线写出的 JSON。

矩阵约定：
  GET /api/quality  /api/kpis  /api/load  /api/stations  /api/alerts
  GET /api/dashboard  整包（大屏一次拉取）
"""
from __future__ import annotations

import json
import os
from pathlib import Path

from flask import Flask, jsonify, send_from_directory

ROOT = Path(__file__).resolve().parent
FRONTEND = ROOT / "frontend"
DATA_FILE = Path(os.environ.get("DASHBOARD_DATA_FILE", ROOT / "mock_data.json"))
QA_FILE = Path(
    os.environ.get(
        "DASHBOARD_QA_FILE",
        ROOT.parent / "bigdata" / "work" / "qa" / "report.json",
    )
)

app = Flask(__name__, static_folder=str(FRONTEND), static_url_path="")

QUALITY_KEYS = (
    "ods_rows",
    "null_start_time",
    "negative_kwh",
    "dup_order_no",
    "orphan_station",
    "dwd_rows",
)


def mock_data() -> dict:
    load_today = [{"hour": h, "kwh": round(18 + (h % 12) * 4.2, 1)} for h in range(24)]
    load_today[8]["kwh"] = 96.0
    load_today[9]["kwh"] = 102.5
    load_today[18]["kwh"] = 1280.5 / 10
    return {
        "generated_at": "2026-09-13T21:00:00",
        "kpis": {
            "today_kwh": 1280.5,
            "today_revenue": 1920.75,
            "idle_piles": 210,
            "busy_piles": 160,
            "fault_piles": 29,
            "peak_hour": "18:00",
            "alert_count": 3,
            "order_count": 16924,
            "total_kwh": 609996.23,
            "total_revenue": 792000.0,
            "active_stations": 72,
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
        "stations": [
            {
                "station_id": 1,
                "name": "深圳市民中心充电站",
                "idle": 2,
                "total": 4,
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
                "forecast": {
                    "h1": {"kwh": 12.0, "idle": 0, "util": 88.0, "is_peak": 1, "congestion": "high"},
                    "h6": {"kwh": 16.5, "idle": 0, "util": 92.0, "is_peak": 1, "congestion": "high"},
                    "h24": {"kwh": 10.2, "idle": 1, "util": 75.0, "is_peak": 0, "congestion": "mid"},
                },
            },
        ],
        "alerts": [
            {
                "station_id": 31,
                "name": "北京南站充电站",
                "horizon": 1,
                "reason": "1小时后预测占用率 88%",
            },
            {
                "station_id": 1,
                "name": "深圳市民中心充电站",
                "horizon": 6,
                "reason": "6小时后预测占用率 83%",
            },
            {
                "station_id": 31,
                "name": "北京南站充电站",
                "horizon": 6,
                "reason": "6小时后预测占用率 92%",
            },
        ],
        "load_hour_avg": load_today,
        "weekday_weekend": {
            "weekday_kwh": 420000.0,
            "weekend_kwh": 180000.0,
            "note": "DWS 分摊电量；weekday=周一至周五，weekend=周六日",
        },
        "regions": [
            {"city": "深圳市", "kwh": 100000.0, "amount": 130000.0, "yuan_per_kwh": 1.3},
            {"city": "北京市", "kwh": 98000.0, "amount": 147000.0, "yuan_per_kwh": 1.5},
        ],
        "pile_types": [
            {"type": "直流", "idle": 220, "busy": 41, "fault": 27, "util": 15.7},
            {"type": "交流", "idle": 82, "busy": 20, "fault": 9, "util": 19.6},
        ],
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


def load_snapshot() -> dict:
    data = mock_data()
    try:
        if DATA_FILE.is_file():
            extra = json.loads(DATA_FILE.read_text(encoding="utf-8"))
            if isinstance(extra, dict) and extra:
                for key, value in extra.items():
                    if isinstance(value, dict) and isinstance(data.get(key), dict):
                        merged = dict(data[key])
                        merged.update(value)
                        data[key] = merged
                    else:
                        data[key] = value
    except (OSError, ValueError):
        pass
    overlay_quality(data)
    kpis = data.setdefault("kpis", {})
    kpis["alert_count"] = len(data.get("alerts") or [])
    return data


@app.get("/api/dashboard")
def api_dashboard():
    return jsonify(load_snapshot())


@app.get("/api/quality")
def api_quality():
    return jsonify(load_snapshot().get("quality", {}))


@app.get("/api/kpis")
def api_kpis():
    return jsonify(load_snapshot().get("kpis", {}))


@app.get("/api/load")
def api_load():
    snap = load_snapshot()
    return jsonify(
        {
            "load_today": snap.get("load_today", []),
            "load_forecast_24h": snap.get("load_forecast_24h", []),
        }
    )


@app.get("/api/stations")
def api_stations():
    return jsonify({"stations": load_snapshot().get("stations", [])})


@app.get("/api/alerts")
def api_alerts():
    return jsonify({"alerts": load_snapshot().get("alerts", [])})


@app.get("/")
def index():
    return send_from_directory(FRONTEND, "index.html")


if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=int(os.environ.get("DASHBOARD_PORT", "8081")),
        debug=False,
    )
