#!/usr/bin/env python3
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch


ROOT = Path(__file__).resolve().parent.parent
METRICS_PATH = ROOT / "bigdata/work/evaluation/model_metrics.json"
OUT_DIR = ROOT / "docs/figures"
HORIZONS = ["1h", "6h", "24h"]


def load_metrics():
    if not METRICS_PATH.is_file():
        raise FileNotFoundError(f"Missing evaluation JSON: {METRICS_PATH}")
    data = json.loads(METRICS_PATH.read_text(encoding="utf-8"))
    expected = {
        "rf": [19.97, 20.51, 22.27],
        "baseline": [27.38, 31.68, 29.90],
        "idle_r2": [0.756, 0.684, 0.663],
    }
    actual = {
        "rf": [round(data["kwh_metrics"][h]["RMSE"], 2) for h in HORIZONS],
        "baseline": [round(data["baseline_metrics"][h]["kwh_RMSE"], 2) for h in HORIZONS],
        "idle_r2": [round(data["idle_metrics"][h]["R2"], 3) for h in HORIZONS],
    }
    if actual != expected:
        raise ValueError(f"Evaluation JSON differs from confirmed values: {actual} != {expected}")
    return data


def save(fig, name):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT_DIR / name, dpi=180, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def rmse_figure(data):
    rf = [data["kwh_metrics"][h]["RMSE"] for h in HORIZONS]
    baseline = [data["baseline_metrics"][h]["kwh_RMSE"] for h in HORIZONS]
    improvement = [27.05, 35.25, 25.52]
    x = np.arange(len(HORIZONS))
    width = 0.34
    fig, ax = plt.subplots(figsize=(16, 9))
    bars_rf = ax.bar(x - width / 2, rf, width, label="Random Forest", color="#3568a8")
    bars_base = ax.bar(x + width / 2, baseline, width, label="Baseline", color="#e28b45")
    for bars in (bars_rf, bars_base):
        for bar in bars:
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.5, f"{bar.get_height():.2f}", ha="center", va="bottom", fontsize=15)
    for i, value in enumerate(improvement):
        ax.text(i, max(rf[i], baseline[i]) + 4.0, f"RMSE Reduction\n{value:.2f}%", ha="center", va="bottom", fontsize=13, color="#333333")
    ax.set_title("kWh Prediction: Random Forest vs Baseline", fontsize=24, pad=20)
    ax.set_ylabel("RMSE", fontsize=17)
    ax.set_xticks(x, HORIZONS, fontsize=16)
    ax.tick_params(axis="y", labelsize=14)
    ax.grid(axis="y", alpha=0.25)
    ax.legend(fontsize=15, frameon=False)
    ax.set_ylim(0, 40)
    fig.tight_layout()
    save(fig, "rf_vs_baseline_rmse.png")


def idle_figure(data):
    values = [data["idle_metrics"][h]["R2"] for h in HORIZONS]
    fig, ax = plt.subplots(figsize=(16, 9))
    bars = ax.bar(HORIZONS, values, color="#5b8fc9", width=0.55)
    for bar in bars:
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.02, f"{bar.get_height():.3f}", ha="center", va="bottom", fontsize=17)
    ax.set_title("Idle Piles Prediction Performance", fontsize=24, pad=20)
    ax.set_ylabel("R²", fontsize=17)
    ax.set_ylim(0, 1)
    ax.tick_params(labelsize=16)
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    save(fig, "idle_r2_by_horizon.png")


def methodology_figure(data):
    fig, ax = plt.subplots(figsize=(16, 9))
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    steps = [
        "Historical Station-hour Data",
        "Exact Timestamp Matching\nt+1h / t+6h / t+24h",
        "Remove Missing Future Targets",
        "Chronological Cutoff\n2026-09-07 16:00",
        "Purged Time Split",
        "Train / Test",
        "Random Forest Evaluation",
    ]
    ys = np.linspace(0.88, 0.30, len(steps))
    for i, (text, y) in enumerate(zip(steps, ys)):
        box = FancyBboxPatch((0.08, y - 0.035), 0.48, 0.07, boxstyle="round,pad=0.012", linewidth=1.5, edgecolor="#3568a8", facecolor="#edf4fb")
        ax.add_patch(box)
        ax.text(0.32, y, text, ha="center", va="center", fontsize=15)
        if i < len(steps) - 1:
            ax.annotate("", xy=(0.32, ys[i + 1] + 0.042), xytext=(0.32, y - 0.042), arrowprops={"arrowstyle": "->", "lw": 1.5, "color": "#777777"})
    ax.text(0.78, 0.84, "Exact Gap Rate", ha="center", fontsize=18, weight="bold")
    ax.text(0.78, 0.75, "1h   100%\n6h   100%\n24h  100%", ha="center", va="top", fontsize=17, linespacing=1.7, color="#2f6f4e")
    ax.text(0.78, 0.45, f"Train Rows: {data['train_rows']:,}\nTest Rows: {data['test_rows']:,}\nPurged Rows: {data['purged_rows']:,}", ha="center", va="top", fontsize=17, linespacing=1.8)
    ax.set_title("Time-aware Model Evaluation", fontsize=24, pad=18)
    fig.tight_layout()
    save(fig, "evaluation_methodology.png")


def main():
    data = load_metrics()
    rmse_figure(data)
    idle_figure(data)
    methodology_figure(data)
    print("Generated:")
    for name in ("rf_vs_baseline_rmse.png", "idle_r2_by_horizon.png", "evaluation_methodology.png"):
        path = OUT_DIR / name
        print(f"  {path}  {plt.imread(path).shape[1]}x{plt.imread(path).shape[0]}")


if __name__ == "__main__":
    main()
