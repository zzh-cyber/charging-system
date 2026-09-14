"""Flask dashboard API for the phase-two demo.

Reads a single JSON snapshot when DASHBOARD_DATA_FILE is set; otherwise serves
deterministic mock data so the dashboard can be demonstrated independently of
Spark/HDFS.  It deliberately has no MySQL dependency.
"""
from pathlib import Path
import json, os
from flask import Flask, jsonify, send_from_directory

ROOT = Path(__file__).resolve().parent
FRONTEND = ROOT / "frontend"
DATA_FILE = Path(os.environ.get("DASHBOARD_DATA_FILE", ROOT / "mock_data.json"))
QA_FILE = Path(os.environ.get("DASHBOARD_QA_FILE", ROOT.parent / "bigdata" / "work" / "qa" / "report.json"))

app = Flask(__name__, static_folder=str(FRONTEND), static_url_path="")

def mock_data():
    return {
        "kpi": {"today_kwh": 1286.4, "today_revenue": 842.60, "active_piles": 47, "stations": 8},
        "quality": {"ods_rows": 24000, "bad_rows": 1680, "clean_rows": 22320,
                    "duplicate_order_no": 240, "orphan_station": 96, "orphan_pile": 112},
        "today_load": [{"hour": h, "kwh": round(22 + 18 * ((h - 12) / 12) ** 2 if h >= 12 else 38 + h * 2, 1)} for h in range(24)],
        "forecast_24h": [{"hour": i + 1, "kwh": round(45 + 12 * ((i % 12) / 12), 1), "util": round(48 + 2.5 * (i % 12), 1)} for i in range(24)],
        "stations": [{"station_id": i, "name": n, "util": u, "idle": max(0, 20 - int(u / 6)), "total": 20}
                     for i, (n, u) in enumerate([("市民中心充电站", 83), ("软件园充电站", 67), ("火车站充电站", 52), ("大学城充电站", 38)], 1)],
        "alerts": [{"station_id": 1, "name": "市民中心充电站", "horizon": 6, "reason": "6小时后预测占用率 83%"}]
    }

def load_data():
    try:
        if DATA_FILE.exists():
            with DATA_FILE.open(encoding="utf-8") as f:
                value = json.load(f)
            if isinstance(value, dict):
                base = mock_data(); base.update(value); return base
    except (OSError, ValueError):
        pass
    base = mock_data()
    # QA is produced separately by Spark; expose it even when no ADS snapshot
    # exists yet.  Keep the dashboard usable with either old or new key names.
    try:
        if QA_FILE.exists():
            qa = json.loads(QA_FILE.read_text(encoding="utf-8"))
            base["quality"].update(qa)
            base["quality"].update({
                "bad_rows": qa.get("bad_rows", sum(qa.get(k, 0) or 0 for k in
                    ("null_start_time", "negative_kwh", "end_before_start", "dup_order_no",
                     "orphan_station", "orphan_pile", "status_contradiction"))),
                "duplicate_order_no": qa.get("duplicate_order_no", qa.get("dup_order_no", 0)),
            })
            if "clean_rows" not in base["quality"]:
                base["quality"]["clean_rows"] = max(0, qa.get("ods_rows", 0) - base["quality"]["bad_rows"])
    except (OSError, ValueError):
        pass
    return base

@app.get("/api/dashboard")
def dashboard():
    return jsonify(load_data())

@app.get("/api/quality")
def quality():
    return jsonify(load_data().get("quality", {}))

@app.get("/")
def index():
    return send_from_directory(FRONTEND, "index.html")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("DASHBOARD_PORT", "8081")), debug=False)
