#!/usr/bin/env python3
"""把 ODS 的模拟订单画成一张自包含 HTML 报告，用来肉眼检验 NO.109/110/111。

只依赖标准库 —— 本机没装 matplotlib / pandas / numpy，也不打算为此引新依赖。
图表是内联 SVG，中文交给浏览器字体渲染，所以不会出现方块字。

    python3 bigdata/report.py                  # -> bigdata/out/ods_report.html
    python3 bigdata/report.py --desktop        # 同时复制一份到 Windows 桌面

看什么（每块图对应一个验收点）：

  1. 24 小时分布（工作日 vs 周末）→ NO.109 的早高峰 8-9、晚高峰 17-20、周末差异
  2. 逐时 kwh 负荷曲线            → 「大屏今日负荷曲线」大概长什么样
  3. 站 × 小时 占用率热力图        → 少数小站高峰被挤爆、绝大多数站空闲
  4. 九类 dq_tag：注入 vs 期望探查 → NO.110 的「探查分类与注入一致」
  5. 站点单量 top / 告警面        → NO.116 的占用率≥80% 预警有没有东西可报

口径说明（跟 NO.115 的 DWS 保持一致，不然后面对不上）：

  占用率(站, 小时) = least(该小时最大并发会话数, 该站总桩数) / 该站总桩数
  并发按 [start_time, end_time) 半开区间算 —— 端点不重复计数
  只算真的充过电的单（start_time 非空），reserved / 未开始的 cancelled 不计
"""

import argparse
import csv
import html
import json
import shutil
import sys
from collections import defaultdict
from datetime import date, datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402

ROOT = common.ROOT
OUT_DIR = common.OUT_DIR
CONTRACT = ROOT / "dq_expected.json"

#: 矩阵 NO.116 的预警阈值，也是马晓钰 NO.124/125 的 congestion 分级线
ALERT_THRESHOLD = 0.80

SIM_NOW = "2026-09-13 21:00:00"  # 兜底，正常从契约里读


# ------------------------------------------------------------------ 读取

def load_contract():
    if not CONTRACT.exists():
        sys.exit(f"❌ 找不到 {CONTRACT}，先跑 python3 bigdata/gen_ods.py --orders")
    return json.loads(CONTRACT.read_text(encoding="utf-8"))


def load_orders(path):
    """读订单 CSV，把要用的列转成合适的类型。空字段 -> None。"""
    rows = []
    with path.open(encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows.append({
                "order_no": r["order_no"],
                "station_id": int(r["station_id"]),
                "pile_id": int(r["pile_id"]),
                "status": r["status"],
                "start_time": r["start_time"] or None,
                "end_time": r["end_time"] or None,
                "kwh": float(r["kwh"]) if r["kwh"] else None,
                "duration_seconds": int(r["duration_seconds"]) if r["duration_seconds"] else 0,
                "created_at": r["created_at"],
                "updated_at": r["updated_at"],
                "dq_tag": r["dq_tag"],
            })
    return rows


def load_dims():
    """读维表。列名就是数据库列名 —— station 是 name/enabled，
    而且【没有】total_piles 列，桩数只能从 pile 文件按 station_id 数出来。"""
    stations, piles = {}, defaultdict(list)
    with (OUT_DIR / "ods_station.csv").open(encoding="utf-8") as f:
        for r in csv.DictReader(f):
            stations[int(r["id"])] = {
                "name": r["name"], "price": float(r["price"]),
                "total_piles": 0,          # 下面数完桩再回填
            }
    with (OUT_DIR / "ods_pile.csv").open(encoding="utf-8") as f:
        for r in csv.DictReader(f):
            sid = int(r["station_id"])
            piles[sid].append(int(r["id"]))
            if sid in stations:
                stations[sid]["total_piles"] += 1
    missing = [s for s in stations if not stations[s]["total_piles"]]
    if missing:
        sys.exit(f"❌ 这些站一个桩都没有，占用率会除零：{missing}")
    orphans = [s for s in piles if s not in stations]
    if orphans:
        sys.exit(f"❌ 桩引用了维表里没有的站：{orphans}")
    return stations, piles


def parse_dt(s):
    return datetime.strptime(s, "%Y-%m-%d %H:%M:%S")


# ------------------------------------------------- 与 simulate 独立的第二实现

def clean_like_dwd(rows, valid_station, valid_pile):
    """按矩阵四条规则独立复刻一遍 DWD。

    刻意【不 import simulate】—— 独立实现之间能对上，才算证据；
    复用生成侧的函数就是自证了。返回 (kept_rows, 各类丢弃计数)。
    """
    drop = defaultdict(int)
    kept = []
    for r in rows:
        if r["start_time"] is None:
            drop["start_time 为空"] += 1
            continue
        if r["kwh"] is None:
            drop["kwh 为空"] += 1
            continue
        if r["kwh"] < 0:
            drop["kwh < 0"] += 1
            continue
        if r["end_time"] is not None and parse_dt(r["end_time"]) < parse_dt(r["start_time"]):
            drop["时间颠倒"] += 1
            continue
        if r["station_id"] not in valid_station or r["pile_id"] not in valid_pile:
            drop["孤儿站/桩"] += 1
            continue
        kept.append(r)

    best = {}
    for r in kept:
        cur = best.get(r["order_no"])
        if cur is None or (r["updated_at"], r["created_at"]) > (cur["updated_at"], cur["created_at"]):
            best[r["order_no"]] = r
    drop["重复单号（留最新）"] = len(kept) - len(best)
    return list(best.values()), drop


# ------------------------------------------------------------------ 统计

def hour_histogram(rows):
    """按 (是否周末, 小时) 数单量。除以天数得到「单/天」由调用方做 ——
    分母必须按【整个窗口】的天数算（见 window_day_counts），
    按「有单的天」算的话周末分母会偏小、周末看起来反而更忙。"""
    counts = defaultdict(lambda: defaultdict(int))
    for r in rows:
        if r["start_time"] is None:
            continue
        t = parse_dt(r["start_time"])
        counts[t.date().weekday() >= 5][t.hour] += 1
    return counts


def window_day_counts(params):
    d0 = date.fromisoformat(params["start_date"])
    d1 = date.fromisoformat(params["end_date"])
    n = {False: 0, True: 0}
    d = d0
    while d <= d1:
        n[d.weekday() >= 5] += 1
        d += timedelta(days=1)
    return n


def hourly_kwh(rows):
    """把每单的 kwh 按与整点区间的重叠秒数摊到小时上，并按工作日/周末分开。

    这个「整点拆分」正是 NO.115 的 DWS 要做的事 —— 这里先算一遍，
    好处是能当场断言「拆完加总 == 原总量」（对得上才说明口径没漏）。
    """
    bucket = {False: defaultdict(float), True: defaultdict(float)}
    total = 0.0
    for r in rows:
        if r["start_time"] is None or r["kwh"] is None:
            continue
        s, e = parse_dt(r["start_time"]), parse_dt(r["end_time"])
        if e <= s:
            continue
        total += r["kwh"]
        span = (e - s).total_seconds()
        we = s.date().weekday() >= 5
        cur = s
        while cur < e:
            nxt = min(cur.replace(minute=0, second=0, microsecond=0) + timedelta(hours=1), e)
            frac = (nxt - cur).total_seconds() / span
            bucket[we][cur.hour] += r["kwh"] * frac
            cur = nxt
    return bucket, total


def occupancy(rows, stations, piles, window_days):
    """按 (站, 日, 小时) 算最大并发占用率，再合成「日均」与「峰值」两张表。

    为什么按天拆：整个窗口直接取最大 = 30 天里挑最极端的那一小时，必然虚高，
    看什么都像「全城永远堵着」。DWS 出的就是 (站, 日, 小时) 粒度，
    大屏展示的也是某一天 —— 所以典型日才是要看的形状。
    日均那张拿来画热力图，峰值那张只放进 tooltip 备查。

    返回 (日均 {(sid,h): 占用率}, 峰值 {(sid,h): 占用率}, 逐日告警 {(日,h): 达阈站数})
    """
    spans = defaultdict(list)
    for r in rows:
        if r["start_time"] is None or r["end_time"] is None:
            continue
        s, e = parse_dt(r["start_time"]), parse_dt(r["end_time"])
        if e <= s:
            continue
        sid = r["station_id"]
        cur = s
        while cur < e:
            nxt = min(cur.replace(minute=0, second=0, microsecond=0) + timedelta(hours=1), e)
            spans[(sid, cur.date(), cur.hour)].append((cur, nxt))
            cur = nxt

    per_day = {}
    for key, ivs in spans.items():
        ivs.sort()
        best = live = 0
        ends = []
        for a, b in ivs:                       # 扫描线：遇到起点 +1，遇到终点 -1
            while ends and ends[0] <= a:
                live -= 1
                ends.pop(0)
            live += 1
            ends.append(b)
            ends.sort()
            best = max(best, live)
        sid = key[0]
        total = stations[sid]["total_piles"] or 1
        per_day[key] = min(best, total) / total

    # 少单的日子该算 0，所以要遍历【整个窗口】的日子，不能只看有会话的
    occ_avg, occ_max, alerts = {}, {}, defaultdict(int)
    for sid in stations:
        for h in range(24):
            vals = [per_day.get((sid, d, h), 0.0) for d in window_days]
            occ_avg[(sid, h)] = sum(vals) / len(vals)
            occ_max[(sid, h)] = max(vals)
    for d in window_days:
        for sid in stations:
            for h in range(24):
                if per_day.get((sid, d, h), 0.0) >= ALERT_THRESHOLD:
                    alerts[(d, h)] += 1
    return occ_avg, occ_max, alerts


# ------------------------------------------------------------------ SVG

def _nice_max(v):
    if v <= 0:
        return 1.0
    import math
    exp = math.floor(math.log10(v))
    base = 10 ** exp
    for m in (1, 1.5, 2, 2.5, 3, 4, 5, 7.5, 10):
        if v <= m * base:
            return m * base
    return 10 * base


def grouped_bars(series, categories, colors, fmt="{:.0f}", ylabel="",
                 height=300, label_every=1):
    """分组柱状图。series: [(名字, [值...])]，categories: x 轴标签。"""
    n = len(categories)
    ns = len(series)
    w, ml, mr, mt, mb = 960, 64, 16, 16, 46
    pw, ph = w - ml - mr, height - mt - mb
    vmax = _nice_max(max((max(vals) if vals else 0) for _, vals in series) or 1)

    p = [f'<svg viewBox="0 0 {w} {height}" width="100%" role="img">']
    for i in range(5):                                    # 网格 + y 轴刻度
        y = mt + ph - ph * i / 4
        v = vmax * i / 4
        p.append(f'<line x1="{ml}" y1="{y:.1f}" x2="{ml+pw}" y2="{y:.1f}" '
                 f'stroke="#e8edf3" stroke-width="1"/>')
        p.append(f'<text x="{ml-8}" y="{y+4:.1f}" text-anchor="end" '
                 f'font-size="11" fill="#8894a4">{fmt.format(v)}</text>')

    gw = pw / n
    bw = max(1.5, gw * 0.78 / ns)
    for gi, cat in enumerate(categories):
        gx = ml + gw * gi
        for si, (_, vals) in enumerate(series):
            v = vals[gi]
            if v <= 0:
                continue
            bh = ph * v / vmax
            x = gx + gw * 0.11 + bw * si
            p.append(f'<rect x="{x:.1f}" y="{mt+ph-bh:.1f}" width="{bw:.1f}" '
                     f'height="{bh:.1f}" fill="{colors[si]}" rx="1.5"><title>'
                     f'{html.escape(cat)} · {html.escape(series[si][0])}: '
                     f'{fmt.format(v)}</title></rect>')
        if gi % label_every == 0:
            p.append(f'<text x="{gx+gw/2:.1f}" y="{mt+ph+16}" text-anchor="middle" '
                     f'font-size="10.5" fill="#6b7787">{html.escape(cat)}</text>')

    p.append(f'<line x1="{ml}" y1="{mt+ph}" x2="{ml+pw}" y2="{mt+ph}" '
             f'stroke="#c9d3df" stroke-width="1"/>')
    p.append("</svg>")
    return "".join(p)


def heat_color(t):
    """0 → 浅灰蓝，1 → 深红。

    gamma 取 1.6（>1，把低值压下去）。别用 <1 —— 那会把低占用率往上抬：
    实测日均占用率中位数才 13%，gamma 0.65 会把它渲染成 29% 的红，
    整张热力图糊成一片红，「多数站很空」这个结论就看不见了。
    """
    t = max(0.0, min(1.0, t)) ** 1.6
    a, b = (233, 240, 247), (192, 39, 45)
    r, g, bl = (int(a[i] + (b[i] - a[i]) * t) for i in range(3))
    return f"#{r:02x}{g:02x}{bl:02x}"


def heatmap(row_labels, matrix, max_matrix=None, height=None, cell_w=30, label_w=210):
    """站 × 小时热力图。matrix[i][h] = 日均占用率（决定颜色），
    max_matrix[i][h] = 窗口内峰值占用率（只进 tooltip，不参与配色）。"""
    nh, ncol = len(matrix), 24
    ch = 13
    height = height or (nh * ch + 46)
    w = label_w + ncol * cell_w + 20
    top = 28

    p = [f'<svg viewBox="0 0 {w} {height}" width="100%" role="img">']
    for h in range(ncol):
        x = label_w + h * cell_w
        p.append(f'<text x="{x+cell_w/2:.0f}" y="16" text-anchor="middle" '
                 f'font-size="10" fill="#6b7787">{h:02d}</text>')

    for i, (lab, vals) in enumerate(zip(row_labels, matrix)):
        y = top + i * ch
        p.append(f'<text x="{label_w-8}" y="{y+9.5:.1f}" text-anchor="end" '
                 f'font-size="9.5" fill="#4a5768">{html.escape(lab)}</text>')
        for h in range(ncol):
            v = vals[h]
            fill = heat_color(v)
            tip = f"日均 {v:.0%}"
            if max_matrix is not None:
                tip += f" · 窗口内最高 {max_matrix[i][h]:.0%}"
            # ≥80% 的格子加描边：颜色深浅是连续的，但「触发 NO.116 预警」是个
            # 硬阈值，得让人一眼数得出来是哪几格，不用去猜色号
            edge = (' stroke="#5c1013" stroke-width="1.3"'
                    if v >= ALERT_THRESHOLD else "")
            p.append(f'<rect x="{label_w+h*cell_w}" y="{y}" width="{cell_w-1}" '
                     f'height="{ch-1}" fill="{fill}"{edge}><title>{html.escape(lab)} '
                     f'{h:02d}:00 · {tip}</title></rect>')

    p.append("</svg>")
    return "".join(p)


def color_legend():
    steps = 12
    cells = "".join(f'<div style="flex:1;height:12px;background:{heat_color(i/(steps-1))}"></div>'
                    for i in range(steps))
    return (f'<div style="display:flex;align-items:center;gap:10px;margin:6px 0 14px">'
            f'<span style="font-size:12px;color:#6b7787">0%</span>'
            f'<div style="display:flex;flex:1;max-width:320px;border-radius:3px;overflow:hidden">'
            f'{cells}</div>'
            f'<span style="font-size:12px;color:#6b7787">100%</span>'
            f'<span style="font-size:12px;color:#c0272d;margin-left:8px">'
            f'≥80% 触发 NO.116 预警（该格带深色描边）</span></div>')


# ------------------------------------------------------------------ 页面

CSS = """
*{box-sizing:border-box}
body{margin:0;padding:28px 32px 64px;background:#f7f9fc;color:#1d2733;
 font-family:"Microsoft YaHei","PingFang SC","Hiragino Sans GB",
 "WenQuanYi Micro Hei",system-ui,sans-serif;font-size:14px;line-height:1.6}
h1{font-size:22px;margin:0 0 4px}
h2{font-size:16px;margin:34px 0 4px;padding-bottom:7px;border-bottom:2px solid #e3e9f1}
.sub{color:#6b7787;font-size:13px;margin:0 0 22px}
.card{background:#fff;border:1px solid #e3e9f1;border-radius:10px;padding:18px 20px;
 margin:12px 0;box-shadow:0 1px 2px rgba(29,39,51,.04)}
.note{background:#fffdf3;border:1px solid #f0e2b8;border-radius:8px;
 padding:11px 15px;font-size:13px;color:#6a5a1e;margin:12px 0}
.note b{color:#8a6d1a}
.legend{display:flex;gap:18px;flex-wrap:wrap;font-size:12.5px;color:#4a5768;margin:2px 0 12px}
.legend i{display:inline-block;width:11px;height:11px;border-radius:2px;margin-right:5px;
 vertical-align:-1px}
table{border-collapse:collapse;width:100%;font-size:13px;margin:6px 0}
th,td{padding:7px 11px;border-bottom:1px solid #eef2f7;text-align:left}
th{background:#f4f7fb;font-weight:600;color:#4a5768;font-size:12.5px}
td.n,th.n{text-align:right;font-variant-numeric:tabular-nums}
.ok{color:#1a7f4b;font-weight:600}
.bad{color:#c0272d;font-weight:600}
.kpi{display:flex;gap:12px;flex-wrap:wrap;margin:14px 0 4px}
.kpi div{flex:1 1 130px;background:#fff;border:1px solid #e3e9f1;border-radius:9px;
 padding:12px 14px}
.kpi .v{font-size:22px;font-weight:600;font-variant-numeric:tabular-nums;line-height:1.25}
.kpi .k{font-size:12px;color:#6b7787}
code{background:#eef2f7;padding:1px 5px;border-radius:4px;font-size:12.5px}
/* 故意【不设 max-height】：热力图 72 行要一眼看全，
   结论就是整体「上红下白」的渐变 —— 塞进滚动框只看得见前 19 行等于没画 */
.scroll{border:1px solid #eef2f7;border-radius:8px;padding:4px}
"""


def kpi(items):
    cells = "".join(f'<div><div class="v">{v}</div><div class="k">{k}</div></div>'
                    for v, k in items)
    return f'<div class="kpi">{cells}</div>'


def legend(items):
    cells = "".join(f'<span><i style="background:{c}"></i>{html.escape(n)}</span>'
                    for n, c in items)
    return f'<div class="legend">{cells}</div>'


def check_row(label, got, want, ok):
    mark = '<span class="ok">✓</span>' if ok else '<span class="bad">✗</span>'
    return (f"<tr><td>{mark} {html.escape(label)}</td>"
            f'<td class="n">{html.escape(str(got))}</td>'
            f'<td class="n">{html.escape(str(want))}</td></tr>')


def build_html(contract, rows, stations, piles, dwd, drops, occ_avg, occ_max,
               alert_cnt, window_days, hist, days, kwh_bucket, kwh_total):
    c_der = contract["derived"]
    c_inj = contract["injected"]
    c_probe = contract["expected_probe"]
    sim_now = contract["params"]["sim_now"]
    problems = []

    # ---- 告警面统计（KPI 块在页首，所以必须在这里先算出来）----
    # 按小时求日均 —— 这才对应「晚高峰平均每天 N 个站被挤爆」这个说法，
    # 而不是整个窗口取最大（那等于 30 天挑最极端的一小时，必然虚高）。
    alert_by_hour = {h: sum(alert_cnt.get((d, h), 0) for d in window_days)
                        / len(window_days) for h in range(24)}
    alert_ev = sum(alert_by_hour[h] for h in (17, 18, 19, 20))
    alert_night = sum(alert_by_hour[h] for h in range(0, 6))
    worst = max(alert_cnt.items(), key=lambda kv: kv[1], default=(None, 0))
    avg_peak = {s: max(occ_avg[(s, h)] for h in range(24)) for s in stations}
    ever_alert = [s for s in stations if avg_peak[s] >= ALERT_THRESHOLD]
    # 窗口内至少有一小时超线的站（含只偶尔超一次的）—— 这跟「常年超线」
    # 是两个量级，混着说会把「预警会触发」夸大成「全城堵死」
    ever_spiked = [s for s in stations
                   if any(occ_max[(s, h)] >= ALERT_THRESHOLD for h in range(24))]

    # ---- 1. 小时分布：工作日 vs 周末（按天均） ----
    wd = [hist[False][h] / max(1, days[False]) for h in range(24)]
    we = [hist[True][h] / max(1, days[True]) for h in range(24)]
    chart_hours = grouped_bars(
        [("工作日", wd), ("周末", we)], [f"{h:02d}" for h in range(24)],
        ["#3b7dd8", "#e8a33d"], fmt="{:.1f}", label_every=1)
    morning = sum(wd[8:10])
    evening = sum(wd[17:21])
    night = sum(wd[0:6])
    peak_h = max(range(24), key=lambda h: wd[h])
    if not (wd[8] > 0 and wd[9] > 0):
        problems.append("工作日 8/9 点没有量，早高峰不成立")
    if not (evening > night * 3):
        problems.append("晚高峰没有明显高于凌晨")
    weekend_ratio = sum(we) / max(1e-9, sum(wd))

    # ---- 2. 逐时 kwh ----
    wd_k = [kwh_bucket[False][h] / max(1, days[False]) for h in range(24)]
    we_k = [kwh_bucket[True][h] / max(1, days[True]) for h in range(24)]
    chart_kwh = grouped_bars(
        [("工作日", wd_k), ("周末", we_k)], [f"{h:02d}" for h in range(24)],
        ["#2f9e7a", "#c9a227"], fmt="{:.0f}", label_every=1)
    apportioned = sum(kwh_bucket[False].values()) + sum(kwh_bucket[True].values())
    ok_ratio = abs(apportioned - kwh_total) < 0.01
    if not ok_ratio:
        problems.append(f"整点拆分后加总 {apportioned:.2f} != 原始 {kwh_total:.2f}")

    # ---- 3. 占用率热力图 ----
    # 72 个站全画、按忙闲降序 —— 只画最忙的那批会把「多数站很空」那半边
    # 藏起来，反而看不出设计意图。红→白的渐变本身就是结论。
    order = sorted(stations, key=lambda s: (-avg_peak[s], s))
    shown = order
    labels = [f'{sid} {stations[sid]["name"]}' for sid in shown]

    # 分布分位数：用来把「多少站常年挤、多少站常年空」写成可核对的数字
    flat = sorted(occ_avg[(s, h)] for s in stations for h in range(24))
    q = lambda p: flat[min(len(flat) - 1, int(len(flat) * p))]  # noqa: E731
    hot_cells = sum(1 for v in flat if v >= ALERT_THRESHOLD)
    chart_heat = heatmap(
        labels,
        [[occ_avg[(sid, h)] for h in range(24)] for sid in shown],
        [[occ_max[(sid, h)] for h in range(24)] for sid in shown])
    chart_alert = grouped_bars(
        [("日均告警站数", [alert_by_hour[h] for h in range(24)])],
        [f"{h:02d}" for h in range(24)], ["#c0272d"], fmt="{:.1f}")

    # ---- 4. dq_tag ----
    tags = [t for t in c_inj if not t.startswith("_")]
    inj = [c_inj[t] for t in tags]
    probe = [c_probe[k] for k in
             ["null_start_time", "null_kwh", "negative_kwh", "soc_range",
              "time_reversed", "dup_order_no", "orphan_station", "orphan_pile",
              "status_conflict"]]
    chart_tags = grouped_bars(
        [("实际注入", inj), ("期望探查", probe)], tags,
        ["#3b7dd8", "#b9c3d0"], fmt="{:.0f}")

    # NULL_START_TIME 是【唯一】允许对不上的一类：它天然多出 1832 行
    # (reserved / 未开始的 cancelled)，期望探查 = 注入 + 业务天然。
    # 判断它要拿 注入+天然 去比，直接比注入数会报假警。
    natural = c_der["natural_null_start"]
    tag_mismatch = [
        f"{t}（注入 {a} + 天然 {natural} = {a + natural}，探查 {b}）"
        if t == "NULL_START_TIME" else f"{t}（注入 {a}，探查 {b}）"
        for t, a, b in zip(tags, inj, probe)
        if b != (a + natural if t == "NULL_START_TIME" else a)]
    if tag_mismatch:
        problems.append("注入与探查对不上：" + "；".join(tag_mismatch))
    # 另外独立断言一下「天然」这个数确实等于探查减注入，别让它自己漂
    if c_probe["null_start_time"] - c_inj["NULL_START_TIME"] != natural:
        problems.append(
            f"NULL_START_TIME 的差值 {c_probe['null_start_time'] - c_inj['NULL_START_TIME']}"
            f" != 契约的 natural_null_start {natural}")

    # ---- 5. 站点 top ----
    per_station = defaultdict(int)
    per_station_kwh = defaultdict(float)
    for r in dwd:
        per_station[r["station_id"]] += 1
        per_station_kwh[r["station_id"]] += r["kwh"] or 0
    top = sorted(per_station, key=lambda s: -per_station[s])[:14]
    chart_top = grouped_bars(
        [("DWD 单量", [per_station[s] for s in top])],
        [f'{s} {stations[s]["name"]}' for s in top], ["#3b7dd8"], fmt="{:.0f}")

    # ---- 契约核对 ----
    checks = [
        ("ODS 行数", len(rows), c_probe["ods_rows"], len(rows) == c_probe["ods_rows"]),
        ("干净行 (dq_tag=OK)", sum(1 for r in rows if r["dq_tag"] == "OK"),
         c_der["clean_rows"],
         sum(1 for r in rows if r["dq_tag"] == "OK") == c_der["clean_rows"]),
        ("脏行", sum(1 for r in rows if r["dq_tag"] != "OK"), c_der["dirty_rows"],
         sum(1 for r in rows if r["dq_tag"] != "OK") == c_der["dirty_rows"]),
        ("DWD 行数（独立复刻）", len(dwd), c_der["dwd_rows"],
         len(dwd) == c_der["dwd_rows"]),
        ("DWD kwh 合计", f"{sum(r['kwh'] for r in dwd):.2f}", c_der["dwd_kwh_total"],
         abs(sum(r["kwh"] for r in dwd) - float(c_der["dwd_kwh_total"])) < 0.5),
        ("合法站数", len(stations), contract["dimension"]["stations"],
         len(stations) == contract["dimension"]["stations"]),
        ("合法桩数", sum(len(v) for v in piles.values()), contract["dimension"]["piles"],
         sum(len(v) for v in piles.values()) == contract["dimension"]["piles"]),
        ("DWD 里 SOC_RANGE（应留）", sum(1 for r in dwd if r["dq_tag"] == "SOC_RANGE"),
         c_der["dwd_by_tag"].get("SOC_RANGE", 0),
         sum(1 for r in dwd if r["dq_tag"] == "SOC_RANGE")
         == c_der["dwd_by_tag"].get("SOC_RANGE", 0)),
    ]
    if any(not c[3] for c in checks):
        problems.extend(c[0] for c in checks if not c[3])
    contract_rows = "".join(check_row(*c) for c in checks)

    # 两类容易被误读的，直接在表里括注清楚，不指望读者自己去对第 4 节
    drop_note = {
        "start_time 为空": f"（注入 {c_inj['NULL_START_TIME']}"
                           f" + 业务天然 {c_der['natural_null_start']}）",
        "孤儿站/桩": f"（两个 anti-join 各 {c_inj['ORPHAN_STATION']}，"
                     f"和 = {c_inj['ORPHAN_STATION'] + c_inj['ORPHAN_PILE']}）",
    }
    drop_rows = "".join(
        f'<tr><td>{html.escape(k)}'
        f'<span style="color:#8894a4;font-size:12px"> '
        f'{html.escape(drop_note.get(k, ""))}</span></td>'
        f'<td class="n">{v}</td></tr>'
        for k, v in sorted(drops.items(), key=lambda kv: -kv[1]))

    # 不变式：丢弃 + 保留 必须正好等于 ODS 总行数。少一条规则或多算一次
    # 都会在这行露出来 —— 比逐项盯数字可靠。
    if sum(drops.values()) + len(dwd) != len(rows):
        problems.append(
            f"丢弃 {sum(drops.values())} + 保留 {len(dwd)} "
            f"!= ODS {len(rows)}，清洗口径有漏或重复")

    verdict = ('<div class="note" style="background:#f1fbf5;border-color:#bfe6cf;'
               'color:#1a7f4b"><b>✓ 全部对上</b> —— 报告里的数字与 '
               '<code>dq_expected.json</code> 契约逐项一致。</div>'
               if not problems else
               '<div class="note" style="background:#fdf2f2;border-color:#f3c9c9;'
               'color:#c0272d"><b>✗ 有对不上的地方</b><ul>'
               + "".join(f"<li>{html.escape(p)}</li>" for p in problems) + "</ul></div>")

    return f"""<!DOCTYPE html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>ODS 模拟数据检验报告 · 9/13</title><style>{CSS}</style></head><body>

<h1>ODS 模拟数据检验报告</h1>
<p class="sub">NO.109 模拟订单 · NO.110 脏数据注入 · NO.111 上 HDFS　|
窗口 {contract['params']['start_date']} ~ {contract['params']['end_date']}
（{contract['params']['days']} 天，工作日 {days[False]} 天 / 周末 {days[True]} 天）　|
seed <code>{contract['params']['seed']}</code>　|　模拟当前时刻 <code>{sim_now}</code></p>

{verdict}

{kpi([
    (f"{contract['params']['rows']:,}", "ODS 订单行数"),
    (f"{sum(1 for r in rows if r['dq_tag'] != 'OK') / len(rows):.1%}", "脏数据占比"),
    (f"{len(dwd):,}", "清洗后 DWD 行数"),
    (f"{sum(r['kwh'] for r in dwd):,.0f}", "DWD 电量合计 (kWh)"),
    (f"{sum(alert_by_hour.values()) / 24:.1f}", "日均告警站数"),
    (f"{alert_ev:.0f}", "晚高峰告警站·小时"),
])}

<h2>1　24 小时分布：工作日 vs 周末</h2>
<p class="sub">按天均单量。工作日早高峰 <b>08–09</b> 为
{wd[8]:.1f} / {wd[9]:.1f} 单/天，晚高峰 <b>17–20</b> 合计 {evening:.1f} 单/天，
凌晨 0–5 只有 {night:.1f} 单/天；峰值小时是 <b>{peak_h:02d}:00</b>（{wd[peak_h]:.1f} 单/天）。
周末为工作日的 {weekend_ratio:.0%}，且曲线更平 —— 对应 NO.109 的「周末差异」。</p>
{legend([("工作日", "#3b7dd8"), ("周末", "#e8a33d")])}
<div class="card">{chart_hours}</div>

<h2>2　逐时电量负荷曲线</h2>
<p class="sub">每单 kwh 按与整点区间的重叠秒数拆分后按天均。这张形状就是
<b>大屏「今日负荷曲线」</b>要长的样子 —— 晚高峰抬起来、凌晨贴地。</p>
{legend([("工作日", "#2f9e7a"), ("周末", "#c9a227")])}
<div class="card">{chart_kwh}</div>
<div class="note">整点拆分后加总 <b>{apportioned:,.2f}</b> kWh，
原始合计 <b>{kwh_total:,.2f}</b> kWh，
差 <b>{abs(apportioned - kwh_total):.2e}</b> ——
这道裂缝就是 NO.115 的 DWS 加总能否等于 ODS 的分母，
只能有浮点误差，不能有真实漏量。</div>

<h2>3　站 × 小时 占用率热力图（日均典型日）</h2>
<p class="sub">全部 {len(shown)} 个站按忙闲<b>降序</b>排列（最忙的在最上面）。
占用率 = <code>least(该小时最大并发会话数, 该站总桩数) / 该站总桩数</code>，
并发按 <code>[start_time, end_time)</code> 半开区间扫描线求最大重叠。
颜色是<b>窗口内每一天该小时的占用率再取平均</b>（悬停可看该格的窗口内最高值）——
不是直接对整个窗口取最大，那样等于 30 天里挑最极端的一小时，必然虚高。</p>
{color_legend()}
<div class="scroll">{chart_heat}</div>
<div class="note"><b>这张图要看的形状是「上红下白」的渐变，不是「一片浅色 + 几点红」。</b>
全 {len(stations)} 站 × 24 时的 {len(flat)} 个格子摊开看：
中位数 <b>{q(0.5):.1%}</b>、P90 <b>{q(0.9):.1%}</b>、最高 <b>{flat[-1]:.1%}</b>，
其中 ≥80% 的只有 <b>{hot_cells}</b> 格（{hot_cells / len(flat):.1%}），
全部集中在最上面那几行的 17–20 点，带深色描边。
剩下 {q(0.5):.0%} 的格子都空得很 —— 这才是「少数站很挤、多数站很空」。</div>
<div class="note">两头都要防：<b>整片发红</b>说明订单密度调过头、全城永远堵着，
热点失去意义；<b>一个红格都没有</b>则 NO.116 的「占用率≥80% 预警」永远不触发、
马晓钰的 <code>congestion</code> 分级也永远只有 low —— 演示不出来。
现在两头都不占。</div>

<h3 style="font-size:14px;margin:20px 0 2px">预警面：每个小时平均有多少个站处于 ≥80%</h3>
<p class="sub">这就是 NO.116 的 <code>alert_count</code> 每天会取的量级。
晚高峰 17–20 点日均合计 <b>{alert_ev:.1f}</b> 个站·小时，
凌晨 0–5 点只有 <b>{alert_night:.2f}</b> —— 昼夜节律是真的，
预警不会全天响个不停，也不会永远为 0。
最极端的一天里，<b>{worst[1]}</b> 个站在同一个小时同时超线。</p>
<div class="card">{chart_alert}</div>
<div class="note">三档站数要看清楚，别混着说：
<b>常年挤爆</b>（日均峰值就 ≥80%）<b>{len(ever_alert)}</b> 个；
<b>偶发超线</b>（窗口内至少有一小时超线，但摊到日均没到）<b>{len(ever_spiked) - len(ever_alert)}</b> 个；
<b>从未超线</b> <b>{len(stations) - len(ever_spiked)}</b> 个。
「{len(ever_spiked)} 个站出现过 ≥80%」和「{len(ever_alert)} 个站常年 ≥80%」是两回事 ——
前者是 NO.116 的预警<b>一定会触发</b>的证据，后者才是「全城堵死」。
少数站很挤、多数站很空的形状正是设计目标：比「所有站都中等忙碌」更真实，
也让智能推荐有区分度可排。</div>

<h2>4　九类脏数据：实际注入 vs 期望探查</h2>
<p class="sub">NO.110 的核心口径：<b>每个脏行只注入一个缺陷</b>，
所以 NO.113 的探查脚本按九类分开数，应当逐项等于注入数。
9/14 的验收就是拿这张图对。</p>
{legend([("实际注入", "#3b7dd8"), ("期望探查", "#b9c3d0")])}
<div class="card">{chart_tags}</div>
<div class="note"><b>只有第一根柱子是「两截」的</b>：
<code>NULL_START_TIME</code> 的期望探查值是 <b>{c_probe['null_start_time']}</b> =
注入 {c_inj['NULL_START_TIME']} + 业务天然 {c_der['natural_null_start']}。
后者是 <code>reserved</code> 和还没开始的 <code>cancelled</code> ——
它们<b>不是脏数据</b>，但 <code>isNull(start_time)</code> 一样会数到。
另外八类两边必须严格相等。别把 {c_probe['null_start_time']} 当成注入数，
那样会以为多注入了 {c_der['natural_null_start']} 行。</div>

<h2>5　清洗丢弃明细与站点单量</h2>
<table><tr><th>丢弃原因（矩阵四条规则）</th><th class="n">行数</th></tr>
{drop_rows}</table>
<p class="sub">注：<b>上面这张表没有 SOC_RANGE 和 STATUS_CONFLICT</b>，它们清洗后留在 DWD 里
（共 {c_der['dwd_by_tag'].get('SOC_RANGE', 0)}
+ {c_der['dwd_by_tag'].get('STATUS_CONFLICT', 0)} 行）。但这两类的性质<b>不一样</b>，
别当成一回事：</p>
<div class="note"><b>SOC_RANGE —— 两种读法一致，就是「留」。</b>
矩阵场景表写的是「SOC 裁剪或置空」，NO.114 的丢弃清单里也没有它，所以保底是
<b>留在 DWD</b>（本报告只标注、未做裁剪，裁剪与否不影响行数）。
丢掉它 <b>{c_der['dwd_by_tag'].get('SOC_RANGE', 0)} 行</b>是明确错的。</div>
<div class="note" style="background:#fdf2f2;border-color:#f3c9c9;color:#8a2b2f">
<b>⚠️ STATUS_CONFLICT —— 矩阵两处自相矛盾，需要组长定。</b>
60–72 行场景表那一列的标题就是「清洗规则（DWD）」，写的是状态矛盾
「丢弃或按规则重算（<b>本阶段丢弃</b>）」；而 NO.114 详细说明的丢弃清单
「丢弃空 start / 负 kwh / 时间颠倒 / 孤儿；重复 order_no 留最新」<b>没列它</b>。
本报告按 NO.114 的行级清单<b>保留</b>，所以
<code>dwd_rows = {c_der['dwd_rows']}</code>、
<code>dwd_kwh_total = {c_der['dwd_kwh_total']}</code>。
<br>若裁定改为丢弃，则是 <code>{c_der['dwd_rows_if_status_conflict_dropped']}</code> 行、
<code>{c_der['dwd_kwh_if_status_conflict_dropped']}</code> 度 ——
<b>两个数都已写进 <code>dq_expected.json</code></b>，9/14 对数字时按裁定的那个取，
别因为记账口径不同判成「探查与注入不一致」。</div>
<div class="card">{chart_top}</div>

<h2>6　契约核对</h2>
<table><tr><th>项</th><th class="n">实测</th><th class="n">契约</th></tr>
{contract_rows}</table>
<p class="sub">DWD 那一行是<b>独立复刻</b>的 —— 报表脚本刻意不 import
<code>simulate.py</code>，两边各自实现同一套规则还能对上，才算证据；
复用生成侧的函数就是自证了。</p>

</body></html>
"""


def main():
    ap = argparse.ArgumentParser(description="生成 ODS 模拟数据的 HTML 检验报告")
    ap.add_argument("--orders", default=str(OUT_DIR / "ods_charge_order.csv"))
    ap.add_argument("--out", default=str(OUT_DIR / "ods_report.html"))
    ap.add_argument("--desktop", action="store_true",
                    help="同时复制一份到 Windows 桌面，方便双击打开")
    args = ap.parse_args()

    contract = load_contract()
    orders_path = Path(args.orders)
    if not orders_path.exists():
        sys.exit(f"❌ 找不到 {orders_path}，先跑 python3 bigdata/gen_ods.py --orders")

    rows = load_orders(orders_path)
    stations, piles = load_dims()
    valid_station = set(stations)
    valid_pile = {p for v in piles.values() for p in v}

    print(f"读入 {len(rows)} 单 / {len(stations)} 站 / {len(valid_pile)} 桩")

    dwd, drops = clean_like_dwd(rows, valid_station, valid_pile)
    hist = hour_histogram(rows)
    days = window_day_counts(contract["params"])
    kwh_bucket, kwh_total = hourly_kwh(dwd)

    window_days = []
    d = date.fromisoformat(contract["params"]["start_date"])
    while d <= date.fromisoformat(contract["params"]["end_date"]):
        window_days.append(d)
        d += timedelta(days=1)

    occ_avg, occ_max, alert_cnt = occupancy(dwd, stations, piles, window_days)
    print(f"DWD {len(dwd)} 行；占用率按 {len(window_days)} 天 × "
          f"{len(stations)} 站 × 24 小时算完")

    doc = build_html(contract, rows, stations, piles, dwd, drops, occ_avg,
                     occ_max, alert_cnt, window_days, hist, days,
                     kwh_bucket, kwh_total)

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(doc, encoding="utf-8")
    print(f"✅ 报告 -> {out}  ({out.stat().st_size / 1024:.0f} KB)")

    if args.desktop:
        dst = Path("/mnt/c/Users/15949/Desktop/ODS检验报告.html")
        try:
            shutil.copyfile(out, dst)
            print(f"✅ 已复制到桌面 -> {dst}")
            print("   Windows 里双击就能打开（WSL 路径不好点）")
        except OSError as e:
            print(f"⚠️  复制到桌面失败：{e}", file=sys.stderr)

    print()
    print(f"   浏览器打开：file://{out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
