#!/usr/bin/env python3
"""生成用户群体管理大屏的两份 ODS CSV（docs/数据.md 三/六 的口径）。

用法：
    python3 bigdata/gen_user_group.py                 # 自检通过后落盘
    python3 bigdata/gen_user_group.py --check-only    # 只重读已落盘的文件复验，不写盘
    python3 bigdata/gen_user_group.py --dry-run       # 生成 + 自检，但不写盘

产出（默认 --out-dir bigdata/ods_user/）：
    ods_ug_user.csv           11247 行 + 表头
    ods_ug_charge_order.csv   ~4.4 万行 + 表头

=============================== 边界（先读这段） ===============================

* **只读** `bigdata/out/ods_{station,pile}.csv` 两份维表。生成前后各算一次 sha256 并断言不变，
  这是「没碰冻结夹具」的可查证据。**绝不写 out/**（`--out-dir` 指向 out/ 会直接 abort）。
* **绝不连 MySQL**。`import common` 会连带 import pymysql，但本脚本从不调用
  `common.connect()`；全程只有读 CSV + 写 CSV。
* **不改** `pipeline.py` 与 qa|clean|dws|ads|train —— 那三份冻结 ODS 喂运营大屏，
  本脚本产出的两份喂用户群体大屏，组长自己接独立 Spark。

=============================== 口径（与 docs/数据.md 对齐） ===============================

【只算已结算】频率、价值、时段、快慢充、常用站点一律只取 `status='settled'`。

【频率】近30天已结算单数：high ≥8 / medium 3~7 / low 1~2 / inactive 0。
   锚点是 `--sim-now`（默认 2026-09-13 21:00:00，与冻结订单文件的 sim_now 一致），
   不是 `NOW()` —— 否则隔天跑出来的占比会漂。

【价值】按近30天消费金额对**全体 11247 人**排名取前 20% / 20~70% / 后 30%。
   **无消费归入低价值** —— 这两句连读只能是「0 消费并列垫底，再对全体人数取名次分位」。
   inactive 的 3037 人近30天金额必然是 0，他们垫底；低价值名额 3375，所以还要 338 个
   有消费的人落进低价值。

   做法是**先定档、再钉金额带**，不是「生成完按名次切」：
   后者人数会自动对上，但第 2249/2250 名可能只差 0.01 元甚至并列，边界一碰就得重抽。
   三条带之间留出真空隙（0→3、40→50、285→320），于是名次分位变成不变量：

       低 (3, 40]    中 [50, 285]    高 [320, 1100]

【快慢充】按人算，用**全部**已结算单（没有时间窗）：快充占比 ≥60% → fast；
   快充 ≤40%（即慢充 ≥60%）→ slow；其他 → mixed。
   ⚠️ mixed 在 n=1/3/5 上**表达不出来**：n=3 时 1/3 和 2/3 都出界，n=5 时 2/5=40% 判 slow、
   3/5=60% 判 fast。所以发牌必须先给「n 能表达 mixed」的人发，见 assign_prefs()。

【常用站点】已结算订单数最多的站，必须**严格唯一**，否则大屏 TOP 排不出来。
   做法是构造而不是碰运气：n≤3 全下在 home 站；n≥4 时让 floor(0.28n) 笔漫游到
   **互不相同**的站、每站 1 笔，于是 home 计数 ≥ n − 0.28n > 1 = 任意漫游站。

【时间窗】订单铺满 90 天（六.6 的 period=90d 要用，且按天补零）。
   近30天桶用 0~27 天前、31~90 天桶用 32~89 天前，**中间 28~31 天留真空** ——
   `end_time >= CURDATE() - INTERVAL 30 DAY` 在边界日上两种口径结果不同，压在边界上占比会抖。

   日子在区间内按「越靠近今天权重越高」抽（day_offsets），于是 90 天趋势是往上走的。
   今天(0)那一档**用**，但要筛起始小时：会话最长 11.8 小时（7kW 慢桩），`sim_now` 是 21:00，
   19:00 起步的单根本充不完，只会变成 charging，而 charging 不是 settled，
   会当场把频率档位算错。跳过整档又会让「近7日」曲线在今天塌下去，是个假的断层。

   **只有今天这一天要卡收工时刻**（cap=sim_now）；更早的日子 cap=None，跨午夜的会话是正常的
   —— 电动车夜里充到第二天很常见。曾经对每天都卡「当天起步当天收工」，结果 18 点以后起步的
   长会话被整片滤掉，晚间时段只剩 19.8%，而 hour_weights 的双峰本来就压在 18/19 点。

============================= 复用了什么（不另造） =============================

common.write_csv()          —— UTF-8 无 BOM / \n 行尾 / 首行表头 / NULL 输出空字段
simulate.COLUMNS            —— 订单 CSV 的**权威列序**（19 业务列 + dq_tag），不重抄
simulate.TAG_OK             —— 干净行哨兵 "OK"
simulate.q2()               —— Decimal 四舍五入。**绝不用内置 round**（那是银行家舍入）
simulate._draw_session()    —— 会话合成，kwh 是权威值、时长由它反推的口径原样继承
simulate.hour_weights()     —— 日内到达分布（工作日双峰 / 周末压平）
simulate.Station / Pile     —— 维表 dataclass
"""

import argparse
import csv
import hashlib
import random
import sys
from collections import Counter, defaultdict
from datetime import datetime, time, timedelta
from decimal import Decimal
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common      # noqa: E402  只用 write_csv，不调 connect()
import simulate    # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
FROZEN_DIR = ROOT / "bigdata" / "out"
FROZEN_FILES = ["ods_station.csv", "ods_pile.csv", "ods_charge_order.csv"]

DEFAULT_OUT_DIR = ROOT / "bigdata" / "ods_user"
DEFAULT_STATION_CSV = FROZEN_DIR / "ods_station.csv"
DEFAULT_PILE_CSV = FROZEN_DIR / "ods_pile.csv"

D = Decimal
Q2 = simulate.q2

# ---------------------------------------------------------------- 硬指标

TOTAL_USERS = 11247
USER_ID_BASE = 100001          # → 100001 ~ 111247 连续
PHONE_PREFIX = "1371"          # → 1371 + 7 位 = 11 位；与 1370000xxxx、13800138001 都不撞

FREQ_TARGET = {"high": 1349, "medium": 3149, "low": 3712, "inactive": 3037}
VALUE_TARGET = {"high": 2249, "medium": 5623, "low": 3375}
PREF_TARGET = {"fast": 4611, "slow": 2924, "mixed": 3712}

assert sum(FREQ_TARGET.values()) == TOTAL_USERS
assert sum(VALUE_TARGET.values()) == TOTAL_USERS
assert sum(PREF_TARGET.values()) == TOTAL_USERS

USER_COLUMNS = ["user_id", "phone", "nickname", "gender", "age_group", "city",
                "vehicle_type", "registration_source", "created_at",
                "last_active_at", "status"]

#: 价值三条带。带间空隙是**构造出来的**（0→3、40→50、285→320），
#: 于是排序后第 2249/2250、7872/7873、8210/8211 名之间不可能并列。
VALUE_BAND = {
    "low":    (D("3.00"),  D("40.00")),
    "medium": (D("50.00"), D("285.00")),
    "high":   (D("320.00"), D("1100.00")),
}
BAND_GAP = D("2.00")           # 兜底放宽的上限，远小于最小空隙 10 元

#: 各属性的精确人数。全部**两两不等**且**没有一个是 100 的整数倍**（一眼假的整百对齐）。
ATTR_COUNTS = {
    "gender": {"male": 5962, "female": 4834, "unknown": 451},
    "age_group": {"under_25": 1401, "25_34": 3298, "35_44": 2854,
                  "45_54": 1812, "55_plus": 1452, "unknown": 430},
    "vehicle_type": {"sedan": 4203, "suv": 3389, "mpv": 1687,
                     "commercial": 1204, "other": 357, "unknown": 407},
    "registration_source": {"android": 4396, "ios": 3693, "web": 1562,
                            "qt": 981, "offline": 264, "unknown": 351},
    "status": {"normal": 10774, "frozen": 473},
    "city": {"深圳": 2413, "广州": 2007, "上海": 1789, "北京": 1541,
             "杭州": 1319, "南京": 1147, "其他": 1031},
}
for _k, _v in ATTR_COUNTS.items():
    assert sum(_v.values()) == TOTAL_USERS, _k

CITY_OF_PREFIX = {"SZ": "深圳", "GZ": "广州", "SH": "上海",
                  "BJ": "北京", "HZ": "杭州", "NJ": "南京"}
OTHER_CITY = "其他"

UNKNOWN_MAX_RATIO = 0.05       # 组长要求 unknown 约 5% 以内，逐维度断言

# ---------------------------------------------------------------- 配额表

#: (频率档, 近30天单数, 历史单数, 人数, 价值档)
#:
#: 这张表是**唯一**的配额来源。三组硬指标由它直接加出来：
#:   频率 = 按第 1 列分档求和        价值 = 按第 5 列分档求和
#: 频率与价值**不独立**（高频天然花得多），所以是联合设计，不是两次独立抽样。
#:
#: 价值档为什么这么配 —— 每一条带对该 n30 都实测可达，不是拍脑袋：
#:   high   = 高频全员 1349 + 中频里 n30∈{6,7} 的 900   → n30≥6，[320,1100] 够得到
#:   medium = 中频 n30∈{3,4,5} 的 2249 + 低频 3374      → [50,285]
#:   low    = 低频里 n30=1 的 338（单笔小额）+ inactive 3037（近30天金额恒为 0）
#:
#: 已结算单总数 = 35,093；再按 --settled-ratio 0.80 补非已结算单 → 总行数 ≈ 4.4 万。
COHORTS = [
    # 高频 1349：n30 8~12，全部高价值
    ("high",      8, 0,  800, "high"),
    ("high",      8, 1,  250, "high"),
    ("high",      8, 2,   50, "high"),
    ("high",      9, 0,  200, "high"),
    ("high",     10, 0,   49, "high"),
    # 中频 3149：n30 6/7 的 900 人进高价值
    ("medium",    6, 0,  700, "high"),
    ("medium",    7, 0,  200, "high"),
    # 中频剩下 2249 人进中价值
    ("medium",    3, 0,  651, "medium"),
    ("medium",    3, 1, 1000, "medium"),
    ("medium",    3, 2,  149, "medium"),
    ("medium",    4, 0,  150, "medium"),
    ("medium",    5, 0,  299, "medium"),
    # 低频 3712：338 人低价值、3374 人中价值
    ("low",       1, 0,  338, "low"),
    ("low",       1, 0, 1500, "medium"),
    ("low",       2, 0, 1874, "medium"),
    # 沉默 3037：近30天 0 笔，31 天外各留 1~2 笔历史单（组长：「31 天外可有历史单」）
    ("inactive",  0, 1, 2400, "low"),
    ("inactive",  0, 2,  637, "low"),
]

#: 近30天 / 历史 的订单天数偏移。中间 28~31 天留真空（见文件头）。
D30_RANGE = (0, 27)
D90_RANGE = (32, 89)

TRIES_PER_HOME = 40            # 每换一个 home 站允许重抽多少次会话
MAX_HOMES = 10                 # 最多换几个 home 站

#: 非已结算订单的构成（占非已结算总量的比例）
NONSETTLED_MIX = {"pending_payment": 0.42, "cancelled": 0.36,
                  "reserved": 0.21, "charging": 0.01}

CANCELLED_STARTED_RATIO = 0.40   # cancelled 里「充上之后才取消」的比例，照既有口径

PREF_FAST, PREF_SLOW, PREF_MIXED = "fast", "slow", "mixed"

#: 价值档 → home 站价格的偏好，按优先级从严格到宽松依次尝试。
#: 这不是装饰：金额带能不能拒绝采样成功，几乎全看 home 站单价挑得对不对。
#:   high 需 ≥1.50 —— n30=6 时 6×36×1.6=346 刚好越过 320，换成 1.3 只有 281，永远过不去
#:   中频中价值需 ≤1.30 —— n30=5 时 5×36×1.6=288 已经越过 285 上沿
#:   低频中价值 n30=1 需 ≥1.50 —— 单笔上限只有 76×1.6=121.6，定价低了够不到 50 下沿
#:   低频低价值需 ≤1.20 —— 单笔要压到 40 以下
PRICE_PREFS = {
    ("high", "any"):   [lambda p: p >= D("1.50"), lambda p: p >= D("1.40")],
    ("medium", "mf"):  [lambda p: p <= D("1.30"), lambda p: p <= D("1.40")],
    ("medium", "lf1"): [lambda p: p >= D("1.50"), lambda p: p >= D("1.40")],
    ("medium", "lf2"): [lambda p: p >= D("1.30")],
    ("low", "lf"):     [lambda p: p <= D("1.20"), lambda p: p <= D("1.30")],
    ("low", "inact"):  [],
}


def price_pref_key(value, freq, n30):
    if value == "high":
        return ("high", "any")
    if value == "low":
        return ("low", "inact") if freq == "inactive" else ("low", "lf")
    if freq == "low":
        return ("medium", "lf1" if n30 == 1 else "lf2")
    return ("medium", "mf")


# ---------------------------------------------------------------- 小工具


def sha256_of(path):
    """分块算 sha256 —— 冻结订单文件有几 MB，别一次读进内存。"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def frozen_fingerprint():
    return {n: sha256_of(FROZEN_DIR / n) for n in FROZEN_FILES
            if (FROZEN_DIR / n).exists()}


def largest_remainder(weights, total):
    """按权重把 total 拆成整数，最大余额法。结果与 dict 迭代顺序无关。"""
    keys = sorted(weights, key=str)
    s = sum(weights[k] for k in keys)
    raw = {k: total * weights[k] / s for k in keys}
    out = {k: int(raw[k]) for k in keys}
    drift = total - sum(out.values())
    order = sorted(keys, key=lambda k: (-(raw[k] - int(raw[k])), str(k)))
    for i in range(abs(drift)):
        out[order[i % len(order)]] += 1 if drift > 0 else -1
    return out


def deal(counts, rng):
    """按 counts 精确发牌，返回打乱后的值列表（精确计数 + 随机分布兼得）。"""
    pool = []
    for v in sorted(counts, key=str):
        pool.extend([v] * counts[v])
    rng.shuffle(pool)
    return pool


def parse_dt(s):
    return datetime.strptime(s, "%Y-%m-%d %H:%M:%S") if s else None


# ---------------------------------------------------------------- 维表


def load_stations(path):
    with open(path, encoding="utf-8", newline="") as f:
        return sorted((simulate.Station(id=int(r["id"]), code=r["station_code"],
                                        name=r["name"], price=D(r["price"]))
                       for r in csv.DictReader(f)), key=lambda s: s.id)


def load_piles(path):
    with open(path, encoding="utf-8", newline="") as f:
        return sorted((simulate.Pile(id=int(r["id"]), station_id=int(r["station_id"]),
                                     code=r["code"], type=r["type"],
                                     power_kw=D(r["power_kw"]))
                       for r in csv.DictReader(f)), key=lambda p: p.id)


def city_of_station(code):
    return CITY_OF_PREFIX.get(code[:2], OTHER_CITY)


# ---------------------------------------------------------------- 用户配额


def build_plan(rng):
    """把 COHORTS 展开成 11247 条用户配额，并打乱槽位。"""
    pool = []
    for freq, n30, nh, cnt, value in COHORTS:
        pool.extend([(freq, n30, nh, value)] * cnt)
    if len(pool) != TOTAL_USERS:
        raise ValueError("COHORTS 展开 %d 人，应为 %d" % (len(pool), TOTAL_USERS))
    rng.shuffle(pool)     # 打散槽位，免得「前 1349 个 id 全是高频」这种一眼假的排列
    return [{"idx": i, "freq": f, "n30": n30, "n_hist": nh, "n_total": n30 + nh,
             "value": v, "pref": None, "k_fast": 0}
            for i, (f, n30, nh, v) in enumerate(pool)]


def can_express_mixed(n):
    """n 笔里能否凑出 40% < 快充占比 < 60%。

    只可能是整数 k 使 0.40 < k/n < 0.60。n=1/3/5 无解：
      n=1 → 0 或 1；n=3 → 1/3 或 2/3；n=5 → 2/5=40% 判 slow、3/5=60% 判 fast。
    """
    return any(0.40 < k / n < 0.60 for k in range(n + 1))


def fast_count_for(pref, n):
    """按偏好反解「这 n 笔里有几笔用快充桩」。"""
    if n <= 0:
        return 0
    if pref == PREF_FAST:
        return max(-((-3 * n) // 5), 1)      # ceil(0.6n)，纯整数避开浮点边界
    if pref == PREF_SLOW:
        return (2 * n) // 5                  # floor(0.4n)
    cands = [k for k in range(n + 1) if 0.40 < k / n < 0.60]
    if not cands:
        raise ValueError("n=%d 表达不出 mixed" % n)
    return min(cands, key=lambda x: (abs(x / n - 0.5), x))


def assign_prefs(plan, rng):
    """先给「n 能表达 mixed」的人发 mixed，剩下的才轮到 fast/slow。

    绝不能反过来随便撒 —— 随机的撒法会让某档漂出区间，而且 n=1/3/5 的人
    若拿到 mixed，组长后端按 k/n 重算出来的偏好就跟配额对不上。
    """
    feasible = [u for u in plan if can_express_mixed(u["n_total"])]
    infeasible = [u for u in plan if not can_express_mixed(u["n_total"])]
    if len(feasible) < PREF_TARGET["mixed"]:
        raise ValueError("能表达 mixed 的只有 %d 人，配额要 %d"
                         % (len(feasible), PREF_TARGET["mixed"]))
    if len(infeasible) > PREF_TARGET["fast"] + PREF_TARGET["slow"]:
        raise ValueError("表达不出 mixed 的有 %d 人，fast+slow 只放得下 %d"
                         % (len(infeasible), PREF_TARGET["fast"] + PREF_TARGET["slow"]))

    # mixed 优先发给人多的（n 越大越能表达出接近 50% 的比例）
    for u in sorted(feasible, key=lambda u: (-u["n_total"], u["idx"]))[:PREF_TARGET["mixed"]]:
        u["pref"] = PREF_MIXED

    rest = [u for u in plan if u["pref"] is None]
    rng.shuffle(rest)
    for u in rest[:PREF_TARGET["fast"]]:
        u["pref"] = PREF_FAST
    for u in rest[PREF_TARGET["fast"]:]:
        u["pref"] = PREF_SLOW

    for u in plan:
        u["k_fast"] = fast_count_for(u["pref"], u["n_total"])
        r = u["k_fast"] / u["n_total"] if u["n_total"] else 0.0
        if u["pref"] == PREF_FAST:
            assert r >= 0.60 - 1e-9, ("fast 档 k/n 越界", u["idx"], r)
        elif u["pref"] == PREF_SLOW:
            assert r <= 0.40 + 1e-9, ("slow 档 k/n 越界", u["idx"], r)
        else:
            assert 0.40 < r < 0.60, ("mixed 档 k/n 越界", u["idx"], r)

    got = Counter(u["pref"] for u in plan)
    for k, v in PREF_TARGET.items():
        assert got[k] == v, "偏好 %s 得到 %d，应为 %d" % (k, got[k], v)


# ---------------------------------------------------------------- 用户属性


def pick_station(u, pool, rng):
    """在池子里按价格偏好过滤后抽一个 home 站（Zipf α=0.35，散得开）。"""
    cands = pool
    for pred in PRICE_PREFS[price_pref_key(u["value"], u["freq"], u["n30"])]:
        hit = [s for s in pool if pred(s.price)]
        if hit:
            cands = hit
            break
    ordered = sorted(cands or pool, key=lambda s: s.id)
    w = [1.0 / ((r + 1) ** 0.35) for r in range(len(ordered))]
    return rng.choices(ordered, weights=w)[0]


def build_users(plan, stations, rng):
    """产出用户行：属性、home 站。created_at / last_active_at 稍后回填。"""
    by_city = defaultdict(list)
    for s in sorted(stations, key=lambda x: x.id):
        by_city[city_of_station(s.code)].append(s)
    all_stations = sorted(stations, key=lambda x: x.id)

    attrs = {k: deal(v, rng) for k, v in ATTR_COUNTS.items()}

    users = []
    for i, u in enumerate(plan):
        city = attrs["city"][i]
        pool = by_city.get(city) or all_stations
        users.append({
            "idx": u["idx"],
            "user_id": USER_ID_BASE + u["idx"],
            "phone": "%s%07d" % (PHONE_PREFIX, u["idx"]),
            "nickname": "用户%04d" % ((USER_ID_BASE + u["idx"]) % 10000),
            "gender": attrs["gender"][i],
            "age_group": attrs["age_group"][i],
            "city": city,
            "vehicle_type": attrs["vehicle_type"][i],
            "registration_source": attrs["registration_source"][i],
            "status": attrs["status"][i],
            "home": pick_station(u, pool, rng),
            "home_pool": pool,
            "freq": u["freq"], "value": u["value"], "pref": u["pref"],
            "n30": u["n30"], "n_hist": u["n_hist"], "n_total": u["n_total"],
            "k_fast": u["k_fast"],
            "created_at": None, "last_active_at": None, "orders": [],
        })

    # 站散度自检：不能一半人挤在同一个站；也不能有站一个用户都没有
    picked = Counter(u["home"].id for u in users)
    top_id, top_n = picked.most_common(1)[0]
    if top_n / len(users) > 0.10:
        raise ValueError("home 站过于集中：站 %d 占 %.1f%%" % (top_id, 100 * top_n / len(users)))
    if len(picked) != len(all_stations):
        raise ValueError("只有 %d 个站被选为 home，共 %d 个" % (len(picked), len(all_stations)))
    return users


# ---------------------------------------------------------------- 订单生成


def _order_no(day, seq):
    """结构化唯一键。前缀用 UG 不用 SIM —— 冻结文件是 SIM{yyyymmdd}{seq:06d}，
    同前缀同日期会撞号，组长那边 groupBy(order_no).count() 会出双行。"""
    return "UG%s%06d" % (day.strftime("%Y%m%d"), seq)


_OFFSET_CACHE = {}


def day_offsets(n, lo, hi, rng):
    """在 [lo, hi] 天前里抽 n 个偏移，**靠近今天**的天权重更高（趋势往上走）。

    d 是「几天前」，所以 d 越小越接近今天。权重取 1 + 2·(hi − d)/span：d=lo（今天）
    拿 3.0，d=hi（最远）拿 1.0。曾经写成 (d − lo) 把方向搞反了 —— 那版让最早的
    那天权重最高，于是 90 天趋势一路往下掉，最近十几天只有个位数订单，看着像数据断了。
    """
    if n <= 0:
        return []
    key = (lo, hi)
    if key not in _OFFSET_CACHE:
        days = list(range(lo, hi + 1))
        span = max(1, hi - lo)
        _OFFSET_CACHE[key] = (days, [1.0 + 2.0 * (hi - d) / span for d in days])
    days, w = _OFFSET_CACHE[key]
    return rng.choices(days, weights=w, k=n)


def draw_one(rng, station, piles_by_station, want_fast, day, cap):
    """在一个站上抽一笔会话。kwh 由 _draw_session 给，时长由它反推。

    `cap` 是**仅对今天**（day == sim_now.date()）生效的最晚结束时刻，其余日子传 None。
    必须区分开：会话最长 11.8 小时（7kW 慢桩），21:00 的 sim_now 下，19:00 起步的单
    到收工都充不完，只能算 charging —— 而这一批必须是 settled，所以要按「起步后能在
    sim_now 前收工」筛可选小时。

    但**跨零点本身不能禁**：电动车夜里充电、第二天早上拔枪是常态。早先版本要求
    「当天起步当天收工」，结果把 19:00 后的长单全筛掉，晚间时段从工作日双峰的 ~34%
    掉到 19.8%。历史日不受 cap 约束，end 可以自然滚到次日。
    """
    pool = piles_by_station[station.id]
    typed = [p for p in pool if (p.type == "fast") == want_fast] or pool
    pile = rng.choice(sorted(typed, key=lambda p: p.id))

    kwh, cap_kwh, ssoc, tsoc, amount, dur_h = simulate._draw_session(rng, pile, station.price)
    secs = int(dur_h * 3600)

    hw = simulate.hour_weights(day.weekday() >= 5)
    if cap is None:
        ok = list(range(24))
    else:
        ok = [h for h in range(24)
              if datetime.combine(day, time(h, 59, 59)) + timedelta(seconds=secs) <= cap]
        if not ok:                  # cap 早于 00:59:59 + 时长，今天排不下这笔
            return None
    hour = rng.choices(ok, weights=[hw[h] for h in ok])[0]
    start = datetime.combine(day, time(hour, rng.randrange(60), rng.randrange(60)))
    end = start + timedelta(seconds=secs)
    return {"station": station, "pile": pile, "kwh": kwh, "cap": cap_kwh,
            "ssoc": ssoc, "tsoc": tsoc, "amount": amount,
            "start": start, "end": end,
            "reserve": start - timedelta(minutes=rng.randint(3, 90))}


def draw_orders(user, home, piles_by_station, rng, sim_now):
    """按 home 站抽满该用户的 n_total 笔已结算单，返回 list[dict]。

    金额带不在这里判 —— 由调用方按 n30 子集的和决定要不要重抽。
    """
    n30, n_total = user["n30"], user["n_total"]
    # 面客站：构造出「home 严格唯一」。n≥4 时 floor(0.28n) 笔漫游到互不相同的站、每站 1 笔，
    # 于是 home 计数 = n − floor(0.28n) ≥ ceil(0.72n)，n≥4 时 > 1 = 任意漫游站。
    roam_n = min(int(0.28 * n_total), len(user["home_pool"]) - 1) if n_total >= 4 else 0
    roamers = rng.sample([s for s in user["home_pool"] if s.id != home.id], roam_n) if roam_n else []
    stations_seq = [home] * (n_total - len(roamers)) + list(roamers)
    fast_seq = [True] * user["k_fast"] + [False] * (n_total - user["k_fast"])
    rng.shuffle(stations_seq)
    rng.shuffle(fast_seq)

    offs = day_offsets(n30, *D30_RANGE, rng) + day_offsets(user["n_hist"], *D90_RANGE, rng)
    out = []
    for j in range(n_total):
        day = (sim_now - timedelta(days=offs[j])).date()
        s = draw_one(rng, stations_seq[j], piles_by_station, fast_seq[j], day,
                     sim_now if day == sim_now.date() else None)
        if s is None:
            return None
        # is30 必须在排序**之前**打：历史单时间更早，排完就跑到前面去了，
        # 到时候 sessions[:n30] 拿到的是历史单，价值带校验整个错位。
        s["is30"] = j < n30
        out.append(s)
    out.sort(key=lambda s: s["start"])
    return out


def settled_rows(user, sessions, seq0):
    rows = []
    for i, s in enumerate(sessions):
        row = {c: None for c in simulate.COLUMNS}
        row["dq_tag"] = simulate.TAG_OK
        row["order_no"] = _order_no(s["start"].date(), seq0 + i)
        row["user_id"] = user["user_id"]
        row["station_id"] = s["station"].id
        row["pile_id"] = s["pile"].id
        row["unit_price"] = s["station"].price
        row["status"] = "settled"
        row["reserve_time"] = s["reserve"]
        row["created_at"] = s["reserve"]
        row["start_time"] = s["start"]
        row["end_time"] = s["end"]
        row["updated_at"] = s["end"]
        row["duration_seconds"] = int((s["end"] - s["start"]).total_seconds())
        row["start_soc"] = s["ssoc"]
        row["battery_capacity_kwh"] = s["cap"]
        row["target_soc"] = s["tsoc"]
        row["kwh"] = s["kwh"]
        row["amount"] = s["amount"]
        row["pay_request_id"] = "PAY" + row["order_no"]
        rows.append(row)
    return rows


def build_settled(users, piles, rng, sim_now, widener):
    """逐用户抽单并钉进价值带（拒绝采样），返回订单行列表。"""
    piles_by_station = defaultdict(list)
    for p in piles:
        piles_by_station[p.station_id].append(p)

    rows, seq = [], 0
    for user in users:
        n30 = user["n30"]
        band = VALUE_BAND[user["value"]] if n30 > 0 else None

        chosen = None
        for relax in (None, BAND_GAP):
            lo, hi = band if band else (D("0"), D("0"))
            if relax is not None and band is not None:
                lo, hi = max(D("0"), lo - relax), hi + relax
            elif relax is not None and band is None:
                break          # 没有带要钉（沉默用户），第一轮就该中
            for attempt in range(MAX_HOMES):
                home = user["home"] if attempt == 0 else pick_station(user, user["home_pool"], rng)
                for _ in range(TRIES_PER_HOME):
                    sessions = draw_orders(user, home, piles_by_station, rng, sim_now)
                    if sessions is None:
                        continue
                    total = sum((s["amount"] for s in sessions if s["is30"]), D("0"))
                    if band is None or lo <= total <= hi:
                        chosen = (home, sessions)
                        break
                if chosen:
                    break
            if chosen:
                if relax is not None:
                    widener.append(user["idx"])
                break
        if chosen is None:
            raise ValueError("用户 %d（%s / %s / n30=%d）金额配不平"
                             % (user["idx"], user["freq"], user["value"], n30))

        user["home"], sessions = chosen
        part = settled_rows(user, sessions, seq)
        seq += len(part)
        user["orders"] = part
        rows.extend(part)
    return rows


def build_nonsettled(users, piles, rng, sim_now, settled_count, ratio, seq0):
    """补足非已结算订单，把 settled 占比拉到 --settled-ratio。

    只发给活跃用户：沉默用户近30天必须一单都没有，否则 last_active_at 与「沉默」自相矛盾。
    """
    piles_by_station = defaultdict(list)
    for p in piles:
        piles_by_station[p.station_id].append(p)

    want = int(round(settled_count * (1 - ratio) / ratio))
    mix = largest_remainder(NONSETTLED_MIX, want)
    active = [u for u in users if u["freq"] != "inactive"]

    rows, seq = [], seq0
    for status in sorted(mix, key=lambda k: (k != "charging", k)):
        for _ in range(mix[status]):
            user = active[rng.randrange(len(active))]
            home = user["home"]
            # 七成落在近30天窗、三成落在历史窗，让 90d 曲线后段不至于全是零
            off = day_offsets(1, *D30_RANGE, rng)[0] if rng.random() < 0.7 \
                else day_offsets(1, *D90_RANGE, rng)[0]
            day = (sim_now - timedelta(days=off)).date()
            # charging 的起止都贴住 sim_now，日期恒为当天 —— 不覆盖的话 order_no 里
            # 内嵌的日期（来自 off）会和真实的 start_time 对不上。
            if status == "charging":
                day = sim_now.date()

            row = {c: None for c in simulate.COLUMNS}
            row["dq_tag"] = simulate.TAG_OK
            row["order_no"] = _order_no(day, seq)
            row["user_id"] = user["user_id"]
            row["station_id"] = home.id
            row["unit_price"] = home.price
            row["status"] = status
            seq += 1

            pile = rng.choice(sorted(piles_by_station[home.id], key=lambda p: p.id))
            row["pile_id"] = pile.id

            started = status == "charging" or (status != "reserved"
                                               and (status != "cancelled"
                                                    or rng.random() < CANCELLED_STARTED_RATIO))
            if not started:
                # 预约/取消但没充上：start/end/SOC 三列全空，kwh/amount 写 0.00 而不是空
                # —— 冻结文件的既有口径。写成空会让下游 isNull(kwh) 多吞这几千行。
                row["reserve_time"] = datetime.combine(
                    day, time(rng.randrange(24), rng.randrange(60), rng.randrange(60)))
                row["created_at"] = row["reserve_time"]
                row["updated_at"] = row["reserve_time"] + timedelta(minutes=rng.randint(5, 600))
                row["duration_seconds"] = 0
                row["kwh"] = D("0.00")
                row["amount"] = D("0.00")
                rows.append(row)
                # 挂回本人身上：fill_user_times() 靠 u['orders'] 倒推 created_at，
                # 只挂 settled 的话，这笔更早的非已结算单会比注册时间还早。
                user["orders"].append(row)
                continue

            want_fast = rng.random() < 0.6
            if status == "charging":
                # 正在充电：end_time 就是 sim_now，**不留空** —— 留空的话 Spark 会用
                # current_timestamp()，13 号的单到 15 号跑就变成 48 小时了。
                pool2 = [p for p in piles_by_station[home.id]
                         if (p.type == "fast") == want_fast] or piles_by_station[home.id]
                pile = rng.choice(sorted(pool2, key=lambda p: p.id))
                kwh, cap, ssoc, tsoc, _, dur_h = simulate._draw_session(rng, pile, home.price)
                start = sim_now - timedelta(minutes=rng.randint(5, 40))
                full = (sim_now + timedelta(seconds=int(dur_h * 3600)) - start).total_seconds()
                frac = max(0.02, min(0.95, (sim_now - start).total_seconds() / max(1.0, full)))
                # 按已充比例缩 kwh，**SOC 三列必须跟着重算**：只缩 kwh 会当场
                # 破坏 kwh == q2((target_soc − start_soc)/100 × 容量) 这条自洽关系。
                # 先用 kwh 反推 target_soc，再由 target_soc 重新导出 kwh，
                # 这样四舍五入后两者仍然严格相等。
                kwh = simulate.q2(kwh * D(str(round(frac, 4))))
                tsoc = simulate.q2(ssoc + (kwh / cap) * D("100"))
                kwh = simulate.q2((tsoc - ssoc) / 100 * cap)
                row["pile_id"] = pile.id
                row["start_time"] = start
                row["end_time"] = sim_now
                row["reserve_time"] = start - timedelta(minutes=rng.randint(3, 40))
                row["created_at"] = row["reserve_time"]
                row["updated_at"] = sim_now
                row["duration_seconds"] = int((sim_now - start).total_seconds())
                row["start_soc"] = ssoc
                row["battery_capacity_kwh"] = cap
                row["target_soc"] = tsoc
                row["kwh"] = kwh
                row["amount"] = simulate.q2(kwh * home.price)
                rows.append(row)
                # 挂回本人身上：fill_user_times() 靠 u['orders'] 倒推 created_at，
                # 只挂 settled 的话，这笔更早的非已结算单会比注册时间还早。
                user["orders"].append(row)
                continue

            s = draw_one(rng, home, piles_by_station, want_fast, day,
                         sim_now if day == sim_now.date() else None)
            if s is None:
                continue
            row["pile_id"] = s["pile"].id
            row["reserve_time"] = s["reserve"]
            row["created_at"] = s["reserve"]
            row["start_time"] = s["start"]
            row["end_time"] = s["end"]
            row["updated_at"] = s["end"]
            row["duration_seconds"] = int((s["end"] - s["start"]).total_seconds())
            row["start_soc"] = s["ssoc"]
            row["battery_capacity_kwh"] = s["cap"]
            row["target_soc"] = s["tsoc"]
            row["kwh"] = s["kwh"]
            row["amount"] = s["amount"]
            rows.append(row)
            # 挂回本人身上：fill_user_times() 靠 u['orders'] 倒推 created_at，
            # 只挂 settled 的话，这笔更早的非已结算单会比注册时间还早。
            user["orders"].append(row)
    return rows


def fill_user_times(users, sim_now, rng):
    """回填 created_at / last_active_at。

    created_at 由**本人最早的订单**倒推 —— 保证 reserve_time ≥ created_at 恒成立，
    不用事后修补。倒推天数上限 45 天，且不早于 2026-06-01。
    """
    floor_dt = datetime(2026, 6, 1)
    cutoff = sim_now - timedelta(days=30)
    for u in users:
        stamps = [o[t] for o in u["orders"] for t in ("reserve_time", "start_time") if o[t]]
        if not stamps:
            raise ValueError("用户 %d 一单都没有" % u["idx"])
        earliest = min(stamps)
        # 注册时间 = 首单往前挪 gap 天再挪掉 1~24 小时。gap 取半正态（σ=9 天），
        # 一半人落在 0~6 天，于是「注册 → 首单」的间隔大多数很短，注册曲线跟着
        # 首单曲线往上走。**不能写成「固定 gap≥3 天」**：那样最近三天永远没有注册
        # （能注册在 09-13 的人首单必须 ≥09-13），9 月中下旬会整片空掉。
        span = max(0, (earliest - floor_dt).days)
        gap = min(span, int(abs(rng.gauss(0, 9))))
        created = earliest - timedelta(days=gap, seconds=rng.randint(3600, 86400))
        u["created_at"] = max(floor_dt, min(created, earliest - timedelta(minutes=1)))

        ends = [o["end_time"] for o in u["orders"] if o["end_time"]]
        u["last_active_at"] = min(max(ends), sim_now) if ends else None

        # 沉默用户：近30天必须没有活动痕迹，last_active_at 留空或早于近 30 天
        if u["freq"] == "inactive" and \
                (u["last_active_at"] is None or u["last_active_at"] >= cutoff):
            u["last_active_at"] = None


# ---------------------------------------------------------------- 自检


def selfcheck(users, orders, stations, piles, sim_now, label=""):
    """对内存行或重读回来的行做同一套断言。失败抛 AssertionError。"""
    errs = []

    def ck(cond, msg):
        if not cond:
            errs.append(msg)

    price_of = {s.id: s.price for s in stations}
    pile_station = {p.id: p.station_id for p in piles}
    ptype = {p.id: p.type for p in piles}
    max_station, max_pile = max(price_of), max(pile_station)

    # ---- A 硬指标 ----
    ck(len(users) == TOTAL_USERS, "用户数 %d != %d" % (len(users), TOTAL_USERS))
    ids = sorted(int(u["user_id"]) for u in users)
    ck(ids == list(range(USER_ID_BASE, USER_ID_BASE + TOTAL_USERS)),
       "user_id 不是 100001..111247 连续无洞")
    phones = [u["phone"] for u in users]
    ck(len(set(phones)) == TOTAL_USERS, "phone 有重复")
    ck(all(len(p) == 11 and p.startswith(PHONE_PREFIX) for p in phones), "phone 不是 11 位 1371 开头")
    ck("13800138001" not in phones, "phone 撞了演示号 13800138001")
    ck(not any(p.startswith("1370000") for p in phones), "phone 落在 1370000xxxx 段")
    ck(all(u["nickname"] for u in users), "nickname 有空值")

    settled = [o for o in orders if o["status"] == "settled"]
    uid_of = {int(u["user_id"]) for u in users}
    ck(all(int(o["user_id"]) in uid_of for o in orders), "订单里有孤儿 user_id")
    ck(len(settled) > 0, "一单 settled 都没有")

    # 频率：按近30天 settled 单数**重算**
    cutoff30 = sim_now - timedelta(days=30)
    per_user = defaultdict(list)
    for o in settled:
        per_user[int(o["user_id"])].append(o)
    freq_got = Counter()
    freq_of = {}
    for u in users:
        uid = int(u["user_id"])
        n = sum(1 for o in per_user[uid] if o["end_time"] >= cutoff30)
        tier = "high" if n >= 8 else "medium" if n >= 3 else "low" if n >= 1 else "inactive"
        freq_of[uid] = tier
        freq_got[tier] += 1
    for k, v in FREQ_TARGET.items():
        ck(freq_got[k] == v, "频率 %s 重算 %d，应为 %d" % (k, freq_got[k], v))

    # 价值：按近30天金额对全体排名**重算**
    amt30 = sorted(((sum((o["amount"] for o in per_user[int(u["user_id"])]
                          if o["end_time"] >= cutoff30), D("0")), int(u["user_id"]))
                    for u in users), key=lambda t: (-t[0], t[1]))
    val_got = Counter()
    for rank, (amt, _) in enumerate(amt30, 1):
        if rank <= 2249:
            val_got["high"] += 1
            ck(amt >= VALUE_BAND["high"][0], "第 %d 名金额 %s 够不到高价值下沿" % (rank, amt))
        elif rank <= 7872:
            val_got["medium"] += 1
            ck(VALUE_BAND["medium"][0] <= amt <= VALUE_BAND["medium"][1],
               "第 %d 名金额 %s 不在中价值带" % (rank, amt))
        else:
            val_got["low"] += 1
            ck(amt <= VALUE_BAND["low"][1], "第 %d 名金额 %s 超出低价值上沿" % (rank, amt))
    for k, v in VALUE_TARGET.items():
        ck(val_got[k] == v, "价值 %s 重算 %d，应为 %d" % (k, val_got[k], v))
    # 三处空隙：边界两侧必须严格分开，否则名次分位会被并列吃掉
    ck(amt30[2248][0] > amt30[2249][0], "高/中价值边界并列")
    ck(amt30[7871][0] > amt30[7872][0], "中/低价值边界并列")
    # 名次是 1 基、下标是 0 基：第 8211 名对应 amt30[8210]。
    ck(amt30[8209][0] > 0 == amt30[8210][0], "低价值/零消费边界不干净")

    # 偏好：按全部 settled 单重算 k/n
    pref_got = Counter()
    for u in users:
        os_ = per_user[int(u["user_id"])]
        ck(len(os_) > 0, "用户 %s 一单 settled 都没有，偏好无从定义" % u["user_id"])
        if not os_:
            pref_got["none"] += 1
            continue
        k = sum(1 for o in os_ if ptype[int(o["pile_id"])] == "fast")
        r = k / len(os_)
        pref_got["fast" if r >= 0.60 - 1e-9 else "slow" if r <= 0.40 + 1e-9 else "mixed"] += 1
    for k, v in PREF_TARGET.items():
        ck(pref_got[k] == v, "偏好 %s 重算 %d，应为 %d" % (k, pref_got[k], v))
    ck(PREF_TARGET["fast"] / TOTAL_USERS != 0.40, "fast 占比正好是 40.0%")

    # ---- 订单总量 ----
    ck(35000 <= len(orders) <= 45000, "订单 %d 行不在 3.5~4.5 万" % len(orders))
    ck(len(settled) / len(orders) >= 0.76,
       "settled 占比 %.3f < 0.76" % (len(settled) / len(orders)))

    # ---- B 冻结自洽关系（逐行） ----
    nos = [o["order_no"] for o in orders]
    ck(len(set(nos)) == len(nos), "order_no 有重复")
    ck(not any(n.startswith("SIM") for n in nos), "order_no 用了 SIM 前缀，会撞冻结文件")
    # order_no 是结构化键（UG + YYYYMMDD + 序号），内嵌日期必须和真实时间自洽：
    # 已充上的看 start_time，没充上的看 reserve_time。charging 分支曾经用 off 抽出来的
    # 日期建号、却把起止改到 sim_now 当天，出过 UG20260831 / start 09-13 这种行。
    for o in orders:
        tag = o["order_no"]
        anchor = o["start_time"] or o["reserve_time"]
        ck(tag[2:10] == anchor.strftime("%Y%m%d"),
           "%s 内嵌日期与 %s 不符" % (tag, anchor.strftime("%Y-%m-%d")))
    for o in orders:
        tag = o["order_no"]
        ck(int(o["station_id"]) <= max_station, "%s station_id 越界" % tag)
        ck(D(str(o["unit_price"])) == price_of[int(o["station_id"])],
           "%s unit_price 与该站价不符" % tag)
        if o["pile_id"]:
            ck(int(o["pile_id"]) <= max_pile, "%s pile_id 越界" % tag)
            ck(pile_station[int(o["pile_id"])] == int(o["station_id"]),
               "%s 的桩不属于该站" % tag)
        if o["start_time"] and o["end_time"]:
            ck(o["end_time"] >= o["start_time"], "%s 时间颠倒" % tag)
            ck(o["reserve_time"] <= o["start_time"], "%s 预约晚于开始" % tag)
            ck(o["duration_seconds"] == int((o["end_time"] - o["start_time"]).total_seconds()),
               "%s duration != end - start" % tag)
            ck(o["end_time"] <= sim_now, "%s end_time 超过 sim_now" % tag)
            ck(o["start_soc"] is not None or o["kwh"] == 0,
               "%s 有起止却没有 SOC" % tag)
            if o["start_soc"] is not None:
                ss, ts = D(str(o["start_soc"])), D(str(o["target_soc"]))
                cap = D(str(o["battery_capacity_kwh"]))
                ck(D("0") <= ss <= 100 and D("0") <= ts <= 100, "%s SOC 越界" % tag)
                ck(ts >= ss, "%s 目标 SOC 低于起始" % tag)
                ck(Q2((ts - ss) / 100 * cap) == D(str(o["kwh"])),
                   "%s kwh != q2(soc差 × 容量)" % tag)
        if o["kwh"] is not None:
            ck(D(str(o["kwh"])) >= 0, "%s kwh 为负" % tag)
            ck(D(str(o["amount"])) == Q2(D(str(o["kwh"])) * price_of[int(o["station_id"])]),
               "%s amount != q2(kwh × 站价)" % tag)

    # status 与字段填法（冻结文件的既有口径）
    for o in orders:
        st, tag = o["status"], o["order_no"]
        if st == "reserved":
            ck(o["start_time"] is None and o["end_time"] is None, "%s reserved 不该有起止" % tag)
            ck(o["duration_seconds"] == 0, "%s reserved 时长该为 0" % tag)
            ck(o["start_soc"] is None and o["battery_capacity_kwh"] is None
               and o["target_soc"] is None, "%s reserved 的 SOC 三列该全空" % tag)
        elif st == "settled":
            ck(o["start_time"] and o["end_time"], "%s settled 缺起止" % tag)
            ck(o["pay_request_id"], "%s settled 缺 pay_request_id" % tag)
            ck(o["dq_tag"] == simulate.TAG_OK, "%s 干净行 dq_tag 不是 OK" % tag)
        elif st == "charging":
            ck(o["end_time"] == sim_now, "%s charging 的 end_time 必须等于 sim_now" % tag)
        if st != "settled":
            ck(o["pay_request_id"] is None, "%s 非 settled 却有支付号" % tag)
        if o["start_time"] is None:
            # 没充上的（reserved / 取消且未充）：kwh 与 amount 必须写 0.00 而不是空
            ck(o["kwh"] is not None and D(str(o["kwh"])) == 0, "%s 没充上却记了电量" % tag)
            ck(D(str(o["amount"])) == 0, "%s 没充上却记了金额" % tag)
        else:
            ck(D(str(o["kwh"])) > 0, "%s 充上了却电量为 0" % tag)

    # ---- C 属性表 ----
    for key, want in ATTR_COUNTS.items():
        got = Counter(u[key] for u in users)
        for v, c in want.items():
            ck(got[v] == c, "%s=%s 得到 %d，应为 %d" % (key, v, got[v], c))
        kids = list(want.values())
        ck(len(set(kids)) == len(kids), "%s 有并列计数" % key)
        ck(all(c % 100 for c in kids), "%s 有整百计数 %s" % (key, kids))
        unk = got.get("unknown", 0)
        ck(unk / len(users) <= UNKNOWN_MAX_RATIO,
           "%s 的 unknown 占 %.2f%% 超过 5%%" % (key, 100 * unk / len(users)))
    ck(all(u["city"] in ATTR_COUNTS["city"] for u in users), "有未知城市")
    ck(all(u["age_group"] and u["registration_source"] and u["vehicle_type"] for u in users),
       "年龄/来源/车型有空")

    for u in users:
        ck(datetime(2026, 6, 1) <= u["created_at"] <= sim_now,
           "用户 %s 的 created_at 越界" % u["user_id"])
    ck(all(u["created_at"] <= datetime(2026, 8, 13)
           for u in users if freq_of[u["user_id"]] == "inactive"),
       "沉默用户 created_at 晚于 2026-08-13，放不下历史单")

    # ---- D 派生一致性 ----
    all_of = defaultdict(list)
    for o in orders:
        all_of[int(o["user_id"])].append(o)
    for u in users:
        uid = int(u["user_id"])
        for o in per_user[uid]:
            ck(o["reserve_time"] >= u["created_at"], "用户 %s 的 settled 单早于注册时间" % u["user_id"])
        # 非已结算单也要查：它们同样带 reserve_time，比注册时间早就是脏数据。
        # 只查 settled 会漏掉 build_nonsettled 的三个分支（曾经漏掉 1997 行）。
        for o in all_of[uid]:
            ck(o["reserve_time"] >= u["created_at"],
               "用户 %s 的 %s 单早于注册时间" % (u["user_id"], o["status"]))
        if freq_of[u["user_id"]] == "inactive":
            ck(u["last_active_at"] is None or u["last_active_at"] < cutoff30,
               "沉默用户 %s 有近30天活跃痕迹" % u["user_id"])
        if u["last_active_at"] and per_user[uid]:
            ck(u["last_active_at"] >= max(o["end_time"] for o in per_user[uid]),
               "用户 %s 的 last_active_at 早于末单" % u["user_id"])
        cnt = Counter(int(o["station_id"]) for o in per_user[uid])
        if cnt:
            ck(list(cnt.values()).count(cnt.most_common(1)[0][1]) == 1,
               "用户 %s 的常用站有并列" % u["user_id"])

    period = Counter()
    for o in settled:
        h = o["start_time"].hour
        period["凌晨" if h < 6 else "上午" if h < 12 else "下午" if h < 18 else "晚间"] += 1
    for k in ("凌晨", "上午", "下午", "晚间"):
        ck(period[k] > 0, "时段 %s 一单都没有" % k)

    by_station = Counter(int(o["station_id"]) for o in orders)
    top = by_station.most_common(1)[0]
    ck(top[1] / len(orders) < 0.15, "单站订单占 %.1f%% 超过 15%%" % (100 * top[1] / len(orders)))
    ck(len(by_station) == max_station, "只有 %d 个站出单，共 %d 个" % (len(by_station), max_station))

    if errs:
        for e in errs[:30]:
            print("  ✗ %s" % e)
        raise AssertionError("%s自检失败，共 %d 条" % (label, len(errs)))
    return True


# ---------------------------------------------------------------- 读写


def read_users(path):
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        r["user_id"] = int(r["user_id"])
        r["created_at"] = parse_dt(r["created_at"])
        r["last_active_at"] = parse_dt(r["last_active_at"])
    return rows


def read_orders(path):
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for c in ("reserve_time", "start_time", "end_time", "created_at", "updated_at"):
            r[c] = parse_dt(r[c])
        for c in ("unit_price", "kwh", "amount", "start_soc",
                  "battery_capacity_kwh", "target_soc"):
            r[c] = D(r[c]) if r[c] else None
        for c in ("id", "user_id", "station_id", "pile_id", "duration_seconds"):
            r[c] = int(r[c]) if r[c] else None
        # CSV 的空字段读回来是 ''，不是 None。不归一的话「某某列必须为空」这类断言
        # 在内存里能过、写出去再读回来就全挂 —— 落盘后的重读复验就是为这个设的。
        for c in ("order_no", "status", "pay_request_id", "dq_tag"):
            r[c] = r[c] or None
    return rows


def write_csv(path, columns, rows):
    common.write_csv(path, columns, [[r[c] for c in columns] for r in rows])


# ---------------------------------------------------------------- 主流程


def main():
    ap = argparse.ArgumentParser(description="生成用户群体大屏的两份 ODS CSV")
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    ap.add_argument("--station-csv", default=str(DEFAULT_STATION_CSV))
    ap.add_argument("--pile-csv", default=str(DEFAULT_PILE_CSV))
    ap.add_argument("--seed", type=int, default=20260913)
    ap.add_argument("--sim-now", default="2026-09-13 21:00:00")
    ap.add_argument("--settled-ratio", type=float, default=0.80)
    ap.add_argument("--order-id-base", type=int, default=400001)
    ap.add_argument("--check-only", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    out_dir = Path(args.out_dir).resolve()
    if out_dir == common.OUT_DIR.resolve() or FROZEN_DIR.resolve() in out_dir.parents:
        raise SystemExit("拒绝执行：--out-dir 不能指向 bigdata/out/（那是冻结夹具）")

    print("=" * 74)
    print("用户群体大屏 ODS 生成器   sim_now=%s  seed=%d" % (args.sim_now, args.seed))
    print("=" * 74)

    before = frozen_fingerprint()
    stations = load_stations(Path(args.station_csv))
    piles = load_piles(Path(args.pile_csv))
    sim_now = datetime.strptime(args.sim_now, "%Y-%m-%d %H:%M:%S")
    print("维表：%d 站 / %d 桩（快 %d / 慢 %d）"
          % (len(stations), len(piles),
             sum(1 for p in piles if p.type == "fast"),
             sum(1 for p in piles if p.type == "slow")))

    user_path = out_dir / "ods_ug_user.csv"
    order_path = out_dir / "ods_ug_charge_order.csv"

    if args.check_only:
        if not user_path.is_file() or not order_path.is_file():
            raise SystemExit("--check-only 需要 %s 已存在" % out_dir)
        users, orders = read_users(user_path), read_orders(order_path)
        selfcheck(users, orders, stations, piles, sim_now, label="[重读文件]")
        print("重读复验通过：用户 %d 行 / 订单 %d 行（settled %d）"
              % (len(users), len(orders),
                 sum(1 for o in orders if o["status"] == "settled")))
        if before != frozen_fingerprint():
            raise SystemExit("❌ 冻结夹具被改动了")
        print("冻结夹具 sha256 未变 ✅")
        return 0

    rng = random.Random(args.seed)
    plan = build_plan(rng)
    assign_prefs(plan, rng)
    users = build_users(plan, stations, rng)

    widener = []
    orders = build_settled(users, piles, rng, sim_now, widener)
    settled_n = len(orders)
    if widener:
        print("⚠ 金额带兜底放宽：%d 人次（放宽上限 %s 元，小于最小空隙 10 元）"
              % (len(widener), BAND_GAP))
    orders += build_nonsettled(users, piles, rng, sim_now, settled_n,
                               args.settled_ratio, settled_n + 1)
    fill_user_times(users, sim_now, rng)

    # 主键从 --order-id-base 起，避开冻结文件占用的 1..20000
    orders.sort(key=lambda o: (o["reserve_time"], o["order_no"]))
    for i, o in enumerate(orders):
        o["id"] = args.order_id_base + i

    urows = [{u_: u.get(u_) for u_ in USER_COLUMNS} for u in users]
    print("生成：用户 %d 行 / 订单 %d 行（settled %d，占 %.1f%%）"
          % (len(urows), len(orders), settled_n, 100 * settled_n / len(orders)))

    selfcheck(urows, orders, stations, piles, sim_now, label="[内存]")
    print("内存自检通过 ✅")
    if before != frozen_fingerprint():
        raise SystemExit("❌ 冻结夹具被改动了")
    print("冻结夹具 sha256 未变 ✅")

    if args.dry_run:
        print("--dry-run：未写盘")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    write_csv(user_path, USER_COLUMNS, urows)
    write_csv(order_path, simulate.COLUMNS, orders)
    print("落盘：%s（%.1f MB）" % (user_path.name, user_path.stat().st_size / 1e6))
    print("      %s（%.1f MB）" % (order_path.name, order_path.stat().st_size / 1e6))

    # 重读文件再验一遍 —— 证的是「写出去的」而不是「内存里的」
    selfcheck(read_users(user_path), read_orders(order_path),
              stations, piles, sim_now, label="[重读文件]")
    print("重读复验通过 ✅")
    if before != frozen_fingerprint():
        raise SystemExit("❌ 冻结夹具被改动了")
    print("冻结夹具 sha256 未变 ✅")
    print("完成。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
