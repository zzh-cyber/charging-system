#!/usr/bin/env python3
"""生成 sql/seed_user_groups_demo.sql —— 用户群体大屏的演示数据（docs/数据.md 二）。

只读演示库，绝不写入：全程只有 SELECT，产出的 .sql 由人工 review 后再执行。

用法：
    python3 sql/gen_user_groups_demo.py             # 探查基线 -> 配平 -> 写文件
    python3 sql/gen_user_groups_demo.py --dry-run   # 配平 + 自检 + 打印，不写文件

=============================== 口径（先读这段） ===============================

【只算已结算】docs/数据.md 三 开头：画像、行为、趋势、价值统计只使用
charge_order.status = 'settled'。频率、价值、时段、快慢充、常用站全部照此。

【近30天】统一 `end_time >= CURDATE() - INTERVAL 30 DAY`，与
server/database.cpp:1959 同款。生成时刻意**不排 n=29/30 天**的已结算单 ——
`CURDATE()` 与 `NOW()` 两种口径在边界日上结果不同，压在边界上占比会抖。
30 天桶只用 n ∈ [1,28]，31~88 天桶用 n ∈ [31,88]；中间 29/30 只放非 settled
状态（这些状态不参与任何统计），白得两天日趋势图的密度。

【价值排名对全体 120 人做，不是只对有消费的人】这是最容易写错的一条。
「无消费归入低价值」+「按近30天消费金额取前20%/20~70%/后30%」两句连读，
只能是「0 消费并列垫底，再对全体人数取名次分位」。只对有消费的 92 人取分位的话，
低价值会变成 60 人（50%），三档全飞出要求。做「四、画像计算」的人必须用
ROW_NUMBER()/名次，**不能**用「金额 ≤ 30 分位」这种值域阈值 —— 后者会让低价值
正好等于 0 消费人数（28）而不是 36。

【状态比例 80/8/12 的口径是「本批新增」】二.2 的 settled 约80% / cancelled 5~10% /
其他约10% 只能在本批上满足。全库口径数学上到不了 80%：存量 337 笔本身只有 54.9%
已结算，(185+k)/(337+k) 是加权平均，随 k 增大只能从 54.9% 逼近本批比例。
全库实际落在约 69%/9%/22%。答辩时主动说明，免得被当成算错。

============================= 配额是怎么推出来的 =============================

全库 120 人 = 存量 40 + 新增 80。**存量 40 的已结算单是固定值**（不许改），
所以三组配额全是从「目标总数 − 存量实测」倒推的。

频率（近30天已结算单数：high≥8 / medium 3~7 / low 1~2 / inactive 0）
    存量实测        high 3 / medium 1 / low 5 / inactive 31
    顶出 3 人       （给 3 个零单用户补近30天的单 -> n30=2 -> low 频）
    存量调整后      high 3 / medium 1 / low 8 / inactive 28
    新增 80 人      high 14 / medium 33 / low 33
    全库            high 17 (14.2%) / medium 34 (28.3%) / low 41 (34.2%) / inactive 28 (23.3%)
                    要求 10~15% / 25~30% / 30~35% / 20~30%  —— 四条全中

价值（近30天金额名次：high 前20% / medium 20~70% / low 后30%）
    0 消费的 28 人（只补了 31~88 天单、n30 仍为 0）必然垫底进低价值。
    低价值名额 36，所以还要 8 个有消费的人进去。名次排布：
        1-3     存量 3 个大户               1006~1284 元（固定，不动）    ┐
        4-24    新增 21 人                  band H  80~400               ┘ high 24 人
        25-84   新增 57 人 + 顶出的 3 人     band M  8~45                 ┐
        85-86   新增 2 人                   band M_LOW 5.50~7.50         │ medium 60 人
        87-92   存量 6 个小户               4.63/0.98/0.59/0.56/0.53/0.48┘（固定）
        93-120  28 个 0 消费                                            low 36 人
    这个排布能成立只靠两条**金额带之间的空隙**，不靠运气：
        band M 下界 8.00  >  存量小户最大值 4.63   （余量 3.37）
        band H 下界 80.00 >  band M 上界 45.00     （余量 35）
    脚本末尾会实测复核这两条，以及第 24/25、84/85 名之间没有并列。

快慢充（已结算单里快充占比 ≥60% fast / ≤40% slow / 其他 mixed，按**用户数**算）
    全库 fast 47 (39.2%) / slow 33 (27.5%) / mixed 40 (33.3%)
    要求 35~45% / 20~30% / 25~35% —— 全中。
    注意 mixed 在 n=3 和 n=5 上**不可能实现**（2:1=66.7%、3:2=60.0% 都够 60%），
    所以定偏好时会先看这个用户的单数能不能表达该偏好，不能就换一档。见 feasible_prefs()。

订单量    本批约 436 笔（全库约 773，落在二.2 的 300~800 内）
    近30天已结算 306 + 31~88天已结算 43 = 349 已结算
    cancelled 36 + 其他状态 51 = 87 非已结算
    本批口径约 80.0% / 8.3% / 11.7%

================================= 几条硬约定 =================================

* 一次性锚点 `SET @anchor = NOW();`。不这么写，脚本跨零点执行时前面几行的
  「近30天」和后面几行会是不同基准，压在边界上的行会飘。
* 幂等靠 phone / order_no + `WHERE NOT EXISTS`，**不用 INSERT IGNORE**
  —— IGNORE 会把外键错 1452、ENUM 非法值、CHECK 违反全部降级成 warning 静默跳过，
  少插几行而脚本报成功，正是二.4 最怕的「残缺而不自知」。
* order_no 用 `UG` + 11 位序号，**不嵌日期**。嵌了日期，隔天重跑就是另一批单号，
  NOT EXISTS 挡不住，数据直接翻倍。日期本来就该在 reserve_time 里。
* `LIKE 'UG%'` 在 utf8mb4_unicode_ci 下**大小写不敏感**，所以 reset/自检圈范围
  一律用 `LIKE BINARY`（或按手机号段圈）。
* station_id / unit_price 一律从 pile JOIN station **推导**，绝不手写 ——
  外键只保证「站存在」，不保证 `order.station_id = pile.station_id`，
  也不保证单价是下单时的站点价（存量 337 笔对得上，是生成器写对了，不是数据库挡的）。
* 站 73 排除：station_code 是 UUID（不是 `SZ001` 那种）、name 叫「1」、单价 2.00
  全库最高、6 根桩全是 idle —— 按 `status='idle'` 挑桩的话它最容易被选中。
* 不碰：用户 13800138001 的余额与现有订单、任何现有 charge_order 行、
  wallet_transactions、pile.current_user_id（全表 0 行非空，存量根本不维护它）。
* created_at 各自早于本人最早订单。存量数据里「订单早于注册」是既有毛病
  （40 人 created_at 全是 2026-09-09，订单却早到 2026-06-01），别再添新的。
"""

import argparse
import random
import sys
from collections import Counter
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "bigdata"))

import common  # noqa: E402  复用同一套连库约定（CHARGING_DB_PASSWORD）

OUT_PATH = ROOT / "sql" / "seed_user_groups_demo.sql"

NEW_PHONE_PREFIX = "1370000"          # 13700000001 ~ 13700000080，实测 0 条占用
ORDER_PREFIX = "UG"                   # 实测 0 条占用（存量 319 笔全是 CD 前缀）
PAY_PREFIX = "PAYUG"
SEED = 20260914
STATION_CODE_RE = r"^[A-Z]{2}[0-9]{3}$"   # 真站编码；73 号是 UUID，靠这个筛掉

TARGET_FREQ = {"high": 17, "medium": 34, "low": 41, "inactive": 28}
TARGET_VALUE = {"high": 24, "medium": 60, "low": 36}
TARGET_PREF = {"fast": 47, "slow": 33, "mixed": 40}
TOTAL_USERS = 120

D30 = (1, 28)        # 近30天桶的天数偏移范围（29/30 留给非 settled）
HIST = (31, 88)      # 31~88 天桶

#: 金额带。**同一段落里的空隙就是价值分层能成立的全部理由**，改动前先读文件头。
#: band H 上界压到 400（不是 500）：n30=8 时单笔分到的金额最大约 400×130/620≈84，
#: 按最低单价 1.1 折合 76 kWh，还在 KWH_MAX=90 以内，不会被夹到而让金额漂。
BAND = {
    "H":      (Decimal("80.00"),  Decimal("400.00")),
    "M_HIGH": (Decimal("8.00"),   Decimal("45.00")),
    "M_LOW":  (Decimal("5.50"),   Decimal("7.50")),
    "HIST":   (Decimal("10.00"),  Decimal("60.00")),   # 只给 inactive，不进近30天口径
}
BAND_M_FLOOR = BAND["M_HIGH"][0]      # 8.00 必须 > 存量小户最大值 4.63
BAND_TOL = Decimal("2.00")            # 折算成电量时被夹过，允许这点漂移

FREQ_SPEC = {"high": [8], "medium": [3, 4, 6, 7], "low": [1, 2]}
NEW_FREQ = {"high": 14, "medium": 33, "low": 33}
N_BAND = {"H": 21, "M_HIGH": 57, "M_LOW": 2}
N_NEW = sum(NEW_FREQ.values())        # 80，与 N_BAND 的总和必须相等

NEW_FROZEN = 6            # 新增用户里的 frozen 数（存量 39 normal / 1 frozen 不动）
RECENT7, RECENT30 = 5, 8  # 近期注册人数，「今日/近7天/近30天新增用户」才不会恒为 0
NON_SETTLED = {"cancelled": 36, "pending_payment": 17, "charging": 17, "reserved": 17}
N_29_30 = 6               # 故意落在 29/30 天的非 settled 单（补日趋势图的空洞）

GENDERS = ["male", "female", "unknown"]
AGE_GROUPS = ["under_25", "25_34", "35_44", "45_54", "55_plus", "unknown"]
VEHICLES = ["sedan", "suv", "mpv", "commercial", "other", "unknown"]
SOURCES = ["android", "ios", "web", "qt", "offline", "unknown"]
PERIODS = ("凌晨", "上午", "下午", "晚间")
PERIOD_HOURS = {"凌晨": (0, 4), "上午": (7, 11), "下午": (13, 17), "晚间": (19, 22)}
CITY_BY_PREFIX = {"SZ": "深圳", "BJ": "北京", "SH": "上海",
                  "GZ": "广州", "HZ": "杭州", "NJ": "南京"}

KWH_MIN, KWH_MAX = Decimal("1.00"), Decimal("90.00")


def q2(v):
    """两位小数的 Decimal。别用 float —— kwh*price 在 double 上会差 0.01，
    存量 337 笔里就有 13 笔是这么来的，自检会误报。"""
    return Decimal(v).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)


def frequency_of(n30):
    if n30 >= 8:
        return "high"
    if n30 >= 3:
        return "medium"
    if n30 >= 1:
        return "low"
    return "inactive"


def pref_of(nf, ns):
    """三.6：快充占比 ≥60% -> fast，慢充 ≥60% -> slow，其他 mixed。

    一单都没有按 mixed 算（表的 ENUM 默认值也是它）。生成器、种子脚本里的
    自检查询、测试脚本三处必须共用这一个口径，否则两边 mixed 会差出几十个人。
    """
    n = nf + ns
    if n == 0:
        return "mixed"
    r = nf / n
    if r >= 0.6:
        return "fast"
    if r <= 0.4:
        return "slow"
    return "mixed"


def feasible_prefs(n):
    """n 笔已结算单能表达出哪些偏好。

    mixed 要求快充占比严格落在 (0.4, 0.6)：n=1 只能 0%/100%；n=3 只能 33%/67%；
    n=5 只能 40%/60%（正好卡边界，算 fast/slow 不算 mixed）。
    所以「想给某个用户定 mixed」之前必须先问单数够不够。
    """
    if n == 0:
        return ["mixed"]
    ok = set()
    for k in range(n + 1):
        r = k / n
        if r >= 0.6:
            ok.add("fast")
        elif r <= 0.4:
            ok.add("slow")
        else:
            ok.add("mixed")
    return sorted(ok)


def allocate_fast(n, pref, rng):
    """n 笔单里安排几笔快充，使落点正好是 pref 那一档。"""
    if n == 0:
        return 0
    if pref == "fast":
        lo = -(-int(round(0.6 * n, 6) * 10) // 10)      # ceil(0.6n)
        return rng.randint(max(lo, 1), n)
    if pref == "slow":
        return rng.randint(0, int(0.4 * n + 1e-9))       # floor(0.4n)
    cands = [k for k in range(n + 1) if 0.4 < k / n < 0.6]
    if not cands:
        raise RuntimeError(f"n={n} 不可能表达 mixed，feasible_prefs 漏了")
    return rng.choice(cands)


# ---------------------------------------------------------------------------
# 1. 只读探查
# ---------------------------------------------------------------------------

def probe(conn):
    out = {}
    with conn.cursor() as cur:
        cur.execute("SELECT DATABASE(), VERSION(), CURDATE()")
        out["db"], out["server"], out["today"] = cur.fetchone()

        cur.execute("SELECT id, phone, status, created_at FROM `user` ORDER BY id")
        users = cur.fetchall()

        cur.execute("""SELECT user_id, COUNT(*), SUM(amount) FROM charge_order
                       WHERE status='settled'
                         AND end_time >= CURDATE() - INTERVAL 30 DAY
                       GROUP BY user_id""")
        d30 = {r[0]: (r[1], q2(r[2])) for r in cur.fetchall()}

        cur.execute("""SELECT user_id, COUNT(*), MAX(end_time) FROM charge_order
                       WHERE status='settled' GROUP BY user_id""")
        dall = {r[0]: (r[1], r[2]) for r in cur.fetchall()}

        cur.execute("SELECT user_id, MIN(reserve_time) FROM charge_order GROUP BY user_id")
        first_any = {r[0]: r[1] for r in cur.fetchall()}

        cur.execute("""SELECT o.user_id, SUM(p.type='fast'), SUM(p.type='slow')
                       FROM charge_order o JOIN pile p ON p.id=o.pile_id
                       WHERE o.status='settled' GROUP BY o.user_id""")
        prefs = {r[0]: (int(r[1]), int(r[2])) for r in cur.fetchall()}

        today = out["today"]
        out["users"] = []
        for uid, phone, status, created in users:
            n30, amt30 = d30.get(uid, (0, Decimal("0.00")))
            nall, last_end = dall.get(uid, (0, None))
            nf, ns = prefs.get(uid, (0, 0))
            out["users"].append({
                "id": uid, "phone": phone, "status": status,
                "n30": n30, "amt30": amt30, "nall": nall, "nf": nf, "ns": ns,
                "last_end_days": (today - last_end.date()).days if last_end else None,
                "first_any_days": (today - first_any[uid].date()).days
                                  if uid in first_any else None,
                "pref": pref_of(nf, ns),
            })

        cur.execute("""SELECT id, station_code, name, price FROM station
                       WHERE station_code REGEXP %s ORDER BY id""", (STATION_CODE_RE,))
        out["stations"] = [{"id": r[0], "code": r[1], "name": r[2],
                            "price": Decimal(str(r[3]))} for r in cur.fetchall()]

        cur.execute("""SELECT p.id, p.station_id, p.code, p.type, p.power_kw, p.status
                       FROM pile p JOIN station s ON s.id=p.station_id
                       WHERE s.station_code REGEXP %s ORDER BY p.id""", (STATION_CODE_RE,))
        out["piles"] = [{"id": r[0], "station_id": r[1], "code": r[2], "type": r[3],
                         "power_kw": Decimal(str(r[4])), "status": r[5]}
                        for r in cur.fetchall()]

        cur.execute("SELECT COUNT(*) FROM `user` WHERE phone LIKE %s",
                    (NEW_PHONE_PREFIX + "%",))
        out["phone_hits"] = cur.fetchone()[0]
        cur.execute("SELECT COUNT(*) FROM charge_order WHERE order_no LIKE BINARY %s",
                    (ORDER_PREFIX + "%",))
        out["order_prefix_hits"] = cur.fetchone()[0]
        cur.execute("SELECT COUNT(*) FROM `user`")
        out["user_count"] = cur.fetchone()[0]
        cur.execute("SELECT COUNT(*) FROM charge_order")
        out["order_count"] = cur.fetchone()[0]
    return out


def assert_baseline(b):
    """基线不符就报错退出 —— 宁可不出文件，也不要静默生成错数据。

    这里每条都是「配额推导的前提」。基线变了（别人注册了用户、跑过订单、
    换了台机器、时间窗口滑了），文件头那套算术就不成立，必须重新推，
    而不是把下面的期望值改一改就当没事。
    """
    p = []
    if b["phone_hits"]:
        p.append(f"手机号段 {NEW_PHONE_PREFIX}% 已占用 {b['phone_hits']} 条，换个号段")
    if b["order_prefix_hits"]:
        p.append(f"单号前缀 {ORDER_PREFIX} 已占用 {b['order_prefix_hits']} 条")
    if b["user_count"] != 40:
        p.append(f"存量用户 {b['user_count']} 人 ≠ 40 —— 全套配额要重推")
    if b["order_count"] != 337:
        p.append(f"存量订单 {b['order_count']} 笔 ≠ 337 —— 全套配额要重推")
    if len(b["stations"]) != 72:
        p.append(f"排除非真站后站点 {len(b['stations'])} 座 ≠ 72 座")

    freq = Counter(frequency_of(u["n30"]) for u in b["users"])
    want = {"high": 3, "medium": 1, "low": 5, "inactive": 31}
    got = {k: freq.get(k, 0) for k in want}
    if got != want:
        p.append(f"存量频率基线 {got} ≠ 实测时的 {want}"
                 f"（多半是有人跑过订单，或时间窗口滑了）")

    with_settled = [u for u in b["users"] if u["nall"] > 0]
    if len(with_settled) != 9:
        p.append(f"有已结算单的存量用户 {len(with_settled)} 人 ≠ 9 人")
    elif any(u["pref"] != "fast" for u in with_settled):
        p.append("存量 9 人现在不全是 fast 了 —— 快慢充配额要重推")

    # 价值名次要能成立，存量的 9 个金额必须**干净地分成两堆**：
    #   三大户 > band H 上界（稳占第 1~3 名）
    #   六小户 < band M_LOW 下界（稳占第 87~92 名）
    # 中间 [5.50, 400] 这段（就是新数据要占的第 4~86 名）**一个存量金额都不能有**，
    # 否则它会插进来把某一档的人挤走。这是整条推理唯一真正脆的地方。
    nums = sorted(u["amt30"] for u in b["users"] if u["amt30"] > 0)
    if len(nums) != 9:
        p.append(f"有消费的存量用户 {len(nums)} 人 ≠ 9 人")
    else:
        big = [a for a in nums if a > BAND["H"][1]]
        mid = [a for a in nums if BAND["M_LOW"][0] <= a <= BAND["H"][1]]
        small = [a for a in nums if a < BAND["M_LOW"][0]]
        if len(big) != 3:
            p.append(f"30天金额高于 band H 上界 {BAND['H'][1]} 的存量用户 "
                     f"{len(big)} 人 ≠ 3 人（大户），第 1~3 名要重排")
        if len(small) != 6:
            p.append(f"30天金额低于 band M_LOW 下界 {BAND['M_LOW'][0]} 的存量用户 "
                     f"{len(small)} 人 ≠ 6 人（小户），第 87~92 名要重排")
        if mid:
            p.append(f"有 {len(mid)} 个存量金额 {mid} 落在新数据的名次区间 "
                     f"[{BAND['M_LOW'][0]}, {BAND['H'][1]}] 内，会把某一档挤走")
        elif big and small:
            b["gap_lo"] = BAND["M_LOW"][0] - small[-1]    # 小户顶到 band M_LOW 的余量
            b["gap_hi"] = big[-1] - BAND["H"][1]          # 大户顶到 band H 的余量
            if b["gap_lo"] <= 0 or b["gap_hi"] <= 0:
                p.append("金额带与存量数据之间没有余量")

    # 每个站都得同时有快桩和慢桩，否则 pick_pile() 会退化到「随便给一根」，
    # 把用户的快慢充偏好连口径一起搞错
    types = {}
    for pl in b["piles"]:
        types.setdefault(pl["station_id"], set()).add(pl["type"])
    bad = [sid for sid, t in types.items() if t != {"fast", "slow"}]
    if bad:
        p.append(f"有 {len(bad)} 个站不是快慢桩都有（如 {bad[:3]}），"
                 f"按偏好挑桩会退化")

    demo = [u for u in b["users"] if u["phone"] == "13800138001"]
    if not demo:
        p.append("演示号 13800138001 不见了")
    else:
        b["demo"] = demo[0]
    return p


# ---------------------------------------------------------------------------
# 2. 配平
# ---------------------------------------------------------------------------

class Plan:
    """一个用户的全部落点。amt30 / nf / ns 由 plan_orders() 回填真实值。"""

    def __init__(self, phone, **kw):
        self.phone = phone
        self.is_new = kw.get("is_new", False)
        self.nickname = kw.get("nickname", "")
        self.gender = kw.get("gender", "unknown")
        self.age_group = kw.get("age_group", "unknown")
        self.city = kw.get("city", "")
        self.vehicle_type = kw.get("vehicle_type", "unknown")
        self.registration_source = kw.get("registration_source", "unknown")
        self.balance = kw.get("balance", Decimal("0.00"))
        self.status = kw.get("status", "normal")
        self.created_days = kw.get("created_days", 120)
        self.freq = kw.get("freq", "inactive")
        self.pref = kw.get("pref")            # None = 还没定，由 assign_prefs() 填
        self.band = kw.get("band")            # H / M_HIGH / M_LOW / None
        self.n30 = kw.get("n30", 0)
        self.n_hist = kw.get("n_hist", 0)
        self.home = kw.get("home")
        self.period = kw.get("period", "晚间")
        self.active_days = kw.get("active_days") or 1
        # —— 由排单回填，verify() 直接读，绝不另算一遍 ——
        self.amt30 = kw.get("amt30", Decimal("0.00"))
        self.nf = kw.get("nf", 0)
        self.ns = kw.get("ns", 0)
        self.order_days = []
        self.pending_specs = []

    @property
    def n_settled(self):
        return self.n30 + self.n_hist


def pick_pref(options, need, rng):
    """从可行偏好里挑缺口最大的那档；并列时随机，免得总是撞到同一个。"""
    return sorted(options, key=lambda o: (-need.get(o, 0), rng.random()))[0]


def _attrs(rng):
    """六列属性的取值。unknown 各留一点，但都不是大头 —— 导入前全表是默认值
    'unknown'，如果导入后还是满屏 unknown，属性分布图就没东西可看。"""
    maybe = rng.random()
    return {
        "gender": "unknown" if maybe < 0.15 else rng.choice(GENDERS[:2]),
        "age_group": "unknown" if maybe < 0.10 else rng.choice(AGE_GROUPS[:5]),
        "vehicle_type": "unknown" if maybe < 0.10 else rng.choice(VEHICLES[:5]),
        "registration_source": "unknown" if maybe < 0.08 else rng.choice(SOURCES[:5]),
    }


def plan_existing(b, rng):
    """存量 40 人：补六列属性 + 铺开注册时间 + 给 31 个零单用户补单。

    只 UPDATE `user` 表自己的列；一行 charge_order 都不删不改。
    9 个已有已结算单的用户一概不加单 —— 他们的频率/价值/偏好是固定的，
    配额就是从他们倒推出来的。

    返回 (user_rows, order_plans, promoted)。user_rows 覆盖全部 40 人
    （都要补属性），order_plans 只覆盖 31 个零单用户（要补单的那批）。
    偏好一律留空（pref=None），由 main() 把存量 31 人和新增 80 人放在一起配。
    """
    zero = [u for u in b["users"] if u["nall"] == 0]
    promoted = zero[:3]                      # 补近30天的单 -> n30=2 -> low 频
    hist = zero[3:]                          # 只补 31~88 天的单 -> n30 仍为 0
    hist_two = {u["id"] for u in hist[:15]}

    user_rows, order_plans = [], []
    for u in b["users"]:
        # created_at 必须早于本人最早订单；存量 1/2/3 号的订单最早到 105 天前
        created = min(max(rng.randint(100, 178), (u["first_any_days"] or 0) + 3), 180)
        station = rng.choice(b["stations"])
        row = dict(phone=u["phone"], is_new=0, nickname="",
                   city=CITY_BY_PREFIX.get(station["code"][:2], "深圳"),
                   balance=Decimal("0.00"), status="normal",
                   created_days=created, active_days=1, **_attrs(rng))

        if u["nall"] == 0:
            is_promo = u in promoted
            n30 = 2 if is_promo else 0
            n_hist = 0 if is_promo else (2 if u["id"] in hist_two else 1)

            home = rng.choice(b["stations"])
            order_plans.append(Plan(
                u["phone"], is_new=False,
                freq="low" if is_promo else "inactive",
                band="M_HIGH" if is_promo else None,   # 顶出的 3 人落 band M
                n30=n30, n_hist=n_hist, created_days=created,
                home=home, period=rng.choice(PERIODS)))
            row["city"] = CITY_BY_PREFIX.get(home["code"][:2], "深圳")
        else:
            # 有单的人：最近活跃时间贴着最后一单，但不能晚于注册
            last = u["last_end_days"] if u["last_end_days"] is not None else 1
            row["active_days"] = max(1, min(last, created - 1))
        user_rows.append(row)
    return user_rows, order_plans, promoted


def plan_new(b, rng):
    """新增 80 人。频率、金额带、偏好三套配额都要落位。

    band 与 freq 是**成对**分配的（不是各自独立抽）：band H 只有 21 个位置，
    14 个高频用户全进去，剩下 7 个从高频往下补，依次类推。

    「近期注册」的 13 人必须**从低频名额里出**（他们只有 1 单，频率天然是 low），
    不能事后另加 —— 那样会把高频/中频的配额挤掉，频率三档直接对不上。
    所以先切成 (频率档, 金额带, 注册天数区间) 三元组，近期那 13 个 slot 直接
    写成 "low"，数量恰好从 low 的 33 里扣掉。
    """
    slots = ([("high", "H", None)] * NEW_FREQ["high"]
             + [("medium", "H", None)] * (N_BAND["H"] - NEW_FREQ["high"])
             + [("medium", "M_HIGH", None)]
             * (NEW_FREQ["medium"] - (N_BAND["H"] - NEW_FREQ["high"]))
             + [("low", "M_HIGH", None)]
             * (NEW_FREQ["low"] - N_BAND["M_LOW"] - RECENT7 - RECENT30)
             + [("low", "M_HIGH", (3, 7))] * RECENT7    # 「近7天新增」有数
             + [("low", "M_HIGH", (10, 28))] * RECENT30  # 「近30天新增」有数
             + [("low", "M_LOW", None)] * N_BAND["M_LOW"])
    assert Counter(f for f, _, _ in slots) == Counter(NEW_FREQ), slots
    assert Counter(bd for _, bd, _ in slots) == Counter(N_BAND), slots
    assert len(slots) == sum(NEW_FREQ.values()) == N_NEW
    rng.shuffle(slots)

    created_range = {"high": (100, 178), "medium": (31, 150), "low": (31, 178)}
    plans = []
    for i, (freq, band, recent) in enumerate(slots):
        phone = f"{NEW_PHONE_PREFIX}{i + 1:04d}"
        station = rng.choice(b["stations"])
        created = rng.randint(*recent) if recent else rng.randint(*created_range[freq])
        n30 = 1 if recent else rng.choice(FREQ_SPEC[freq])

        if band == "M_LOW":
            n30 = 1                           # 只有 1 单，金额才压得进 5.50~7.50
        n30 = min(n30, created - 2)           # 订单必须晚于注册

        plans.append(Plan(
            phone, is_new=True,
            nickname="用户" + phone[-4:],      # 照服务端约定：'用户' + 末四位
            city=CITY_BY_PREFIX.get(station["code"][:2], "深圳"),
            balance=q2(Decimal(rng.choice([0, 0, 20, 50, 100, 200, 300, 500]))
                       + Decimal(rng.randint(0, 99)) / 100),
            status="frozen" if i < NEW_FROZEN else "normal",
            created_days=created, freq=frequency_of(n30), band=band,
            n30=n30, n_hist=0, home=station, period=rng.choice(PERIODS),
            **_attrs(rng)))
    return plans


def assign_prefs(plans, need, rng):
    """给这批人定偏好，返回剩余的缺口。存量 31 人和新增 80 人各调一次。

    分两步，顺序不能反：
      1. 先把 mixed 派给「表达得出 mixed」的人（n≥2 的那些）。
      2. 剩下的都只能 fast/slow —— 这两档对任何 n≥1 的人都可行，随便填。

    一步到位地「缺口最大的先派」是错的：单数最多的人也最全能，会先把 fast 抢光，
    等轮到 mixed 时只剩下一单一单的人，而 n=1 根本表达不出 mixed（见 feasible_prefs），
    于是 mixed 缺口永远补不上、fast/slow 反而超额。
    """
    capable = [p for p in plans if "mixed" in feasible_prefs(p.n_settled)]
    rng.shuffle(capable)
    capable.sort(key=lambda p: -p.n_settled)      # 单数多的先占，兜底更稳
    want = min(max(need.get("mixed", 0), 0), len(capable))
    for p in capable[:want]:
        p.pref = "mixed"
    need["mixed"] -= want

    rest = [p for p in plans if p.pref is None]
    rng.shuffle(rest)
    for p in rest:
        opts = [o for o in feasible_prefs(p.n_settled) if o != "mixed"]
        p.pref = pick_pref(opts, need, rng)
        need[p.pref] -= 1
    return need


def assign_non_settled(plans, rng):
    """把 36/17/17/17 笔非已结算单摊到**新增**用户头上（存量一概不加）。

    均下来每人 1.09 笔，所以有人 1 笔有人 2 笔。其中 N_29_30 笔特意落在
    29/30 天前 —— 那两个日子不能放已结算单（见文件头），放非已结算刚好补上
    日趋势图的密度。
    """
    statuses = [s for s, c in NON_SETTLED.items() for _ in range(c)]
    rng.shuffle(statuses)
    targets = [p for p in plans if p.is_new]
    for i, st in enumerate(statuses):
        p = targets[i % len(targets)]
        if st in LIVE:
            # 进行中的单：几分钟前开始，见 TS 的注释
            p.pending_specs.append(
                {"status": st, "day": 1, "anchor_min": rng.randint(*LIVE_MINUTES)})
        # 29/30 天那两个洞只有注册够久的人才填得上；其余按注册时间收口，
        # 否则近期注册的人会冒出一笔「注册前」的订单
        elif i < N_29_30 and p.created_days > 30:
            p.pending_specs.append(
                {"status": st, "day": rng.choice([29, 30]), "anchor_min": None})
        else:
            p.pending_specs.append(
                {"status": st, "anchor_min": None,
                 "day": rng.randint(1, min(HIST[1], p.created_days - 1))})
    return Counter(statuses)


# ---------------------------------------------------------------------------
# 3. 排订单
# ---------------------------------------------------------------------------

def split_amount(total, n, rng):
    """把总额拆成 n 份（带抖动），最后一份吃掉舍入误差，保证总和不变。"""
    total = q2(total)
    if n == 1:
        return [total]
    w = [Decimal(rng.randint(70, 130)) for _ in range(n)]
    s = sum(w)
    parts = [q2(total * x / s) for x in w[:-1]]
    parts.append(q2(total - sum(parts)))
    return parts


#: 进行中的单：预约/充电开始于 10 分钟 ~ 6 小时前。再久就不像「正在充」了。
#: 这类单不能按「N 天前」排 —— 天偏移最小是 1，会排成「充了 28 天的电」；
#: 用 0 又落在今天、可能越过 @anchor 变成未来时间。所以改走 anchor_min
#: （几分钟前），见产出文件里派生表的 start_at。
LIVE = ("charging", "reserved")
LIVE_MINUTES = (10, 360)


def _days(rng_range, plan):
    """把天数偏移区间收到「注册之后」。

    近期注册的人（3~7 天前、10~28 天前）本来只该有近几天的单，直接抽
    D30=(1,28) 会给他排出「注册前就充过电」的订单 —— 存量数据里
    「订单早于注册」本来就是既有毛病，别再添新的。
    """
    lo, hi = rng_range
    hi = min(hi, plan.created_days - 1)
    if hi < lo:
        raise RuntimeError(f"{plan.phone} 注册才 {plan.created_days} 天前，"
                           f"排不出 {rng_range} 区间里的单")
    return lo, hi


def plan_orders(plans, b, rng):
    """给每个用户排订单，并把**落地的真实金额与快慢充笔数写回 plan**。

    金额是「先定桶的总额、再拆到各单、再按该单站点的单价折成电量」：
    amount 决定价值名次，必须严格控制住**桶总额**落在 band 里。
    单笔各自抽 band 是错的 —— n30=2 时两笔各自顶上界，合计会到 2×上界直接串档。
    折成电量时可能被 [KWH_MIN, KWH_MAX] 夹一下，所以最终金额会漂一丁点
    （band H 最坏几十元，band 之间留了 35 和 3.37 的余量，吃得住）。
    真正生效的值写回 plan.amt30，verify() 读的就是它 —— 绝不另算一遍。
    """
    by_station = {}
    for p in b["piles"]:
        by_station.setdefault(p["station_id"], []).append(p)
    price_of = {s["id"]: s["price"] for s in b["stations"]}
    stations = b["stations"]
    orders = []
    seq = 0

    def emit(plan, status, *, start_days, start_time_s, dur_s, gap_s, kwh, pile,
             anchor_min=None):
        nonlocal seq
        seq += 1
        orders.append({
            "seq": seq, "order_no": f"{ORDER_PREFIX}{seq:011d}", "phone": plan.phone,
            "pile_id": pile["id"], "status": status, "kwh": q2(kwh),
            "start_days": start_days, "start_time_s": start_time_s,
            "dur_s": dur_s, "gap_s": gap_s, "anchor_min": anchor_min,
            "pay_no": f"{PAY_PREFIX}{seq:09d}" if status == "settled" else None,
        })
        plan.order_days.append(start_days)

    def pick_pile(station_id, want_type, status, rng):
        pool = [p for p in by_station[station_id] if p["type"] == want_type]
        if not pool:                 # 每站都同时有快慢桩，理论上到不了这
            pool = by_station[station_id]
        if status in ("charging", "reserved"):
            # 存量 63 根 busy 桩上挂着活单，别再往上叠
            idle = [p for p in pool if p["status"] == "idle"]
            if idle:
                pool = idle
        return rng.choice(pool)

    def start_time(plan, rng):
        p = plan.period if rng.random() < 0.55 else rng.choice(PERIODS)
        lo, hi = PERIOD_HOURS[p]
        return f"{rng.randint(lo, hi):02d}:{rng.randint(0, 59):02d}:{rng.randint(0, 59):02d}"

    for plan in plans:
        # —— 已结算：先按偏好把快/慢桩位定好，再逐单配金额与站点 ——
        n_settled = plan.n_settled
        if n_settled:
            k = allocate_fast(n_settled, plan.pref, rng)
            flags = [True] * k + [False] * (n_settled - k)
            rng.shuffle(flags)
        else:
            flags = []

        slots = []
        if plan.n30:
            lo, hi = BAND[plan.band]
            total = q2(lo + (hi - lo) * Decimal(rng.randint(0, 1000)) / 1000)
            for amt in split_amount(total, plan.n30, rng):
                slots.append((amt, rng.randint(*_days(D30, plan)), True))
        if plan.n_hist:
            lo, hi = BAND["HIST"]
            total = q2(lo + (hi - lo) * Decimal(rng.randint(0, 1000)) / 1000)
            for amt in split_amount(total, plan.n_hist, rng):
                slots.append((amt, rng.randint(*_days(HIST, plan)), False))
        assert len(slots) == n_settled, (plan.phone, len(slots), n_settled)

        for (amount, days, is_30d), is_fast in zip(slots, flags):
            station = plan.home if rng.random() < 0.7 else rng.choice(stations)
            price = price_of[station["id"]]
            kwh = min(max(q2(amount / price), KWH_MIN), KWH_MAX)
            pile = pick_pile(station["id"], "fast" if is_fast else "slow",
                             "settled", rng)
            dur = max(int(kwh / pile["power_kw"] * 3600), 60)
            t = start_time(plan, rng)
            h, m, _ = (int(x) for x in t.split(":"))
            if h * 3600 + m * 60 + dur >= 86400:
                days = max(days + 1, 2)   # 跨零点会落到今天，可能越过 @anchor
            emit(plan, "settled", start_days=days, start_time_s=t, dur_s=dur,
                 gap_s=rng.randint(300, 2400), kwh=kwh, pile=pile)
            if is_30d:
                plan.amt30 += q2(kwh * price)
            if is_fast:
                plan.nf += 1
            else:
                plan.ns += 1

        # —— 非已结算：只在新增用户身上，不碰存量 ——
        for spec in plan.pending_specs:
            status, amin = spec["status"], spec["anchor_min"]
            station = plan.home
            # 一律慢桩：这些单要么 0 电量，要么时长要跟桩功率自洽，
            # 快桩配长时长会算出几百上千度电
            pile = pick_pile(station["id"], "slow", status, rng)
            if status == "charging":
                # 进行中：电量 = 已充时长 × 桩功率，与 duration_seconds 自洽
                dur = amin * 60
                kwh = q2(Decimal(dur) / 3600 * pile["power_kw"])
            elif status in ("cancelled", "reserved"):
                # 取消：没充过电；预约：还没开始充。两者电量金额都是 0（照存量约定）
                kwh, dur = Decimal("0.00"), 0
            else:                                  # pending_payment
                kwh = q2(Decimal(rng.randint(300, 3000)) / 100)
                dur = max(int(kwh / pile["power_kw"] * 3600), 60)
            emit(plan, status, start_days=max(spec["day"], 1),
                 start_time_s=start_time(plan, rng), dur_s=dur,
                 gap_s=rng.randint(300, 2400), kwh=kwh, pile=pile,
                 anchor_min=amin)

        if plan.order_days:
            plan.active_days = min(plan.order_days)
        plan.active_days = max(1, min(plan.active_days, plan.created_days - 1))

    return orders


# ---------------------------------------------------------------------------
# 4. 自检：按 SQL 同款口径把三组比例重算一遍
# ---------------------------------------------------------------------------

def verify(user_rows, plans, b):
    """对全体 120 人重算三组比例。对不上就不写文件。

    这里算的必须和产出 SQL 里那段自检查询**口径一致**，否则自检通过、
    真跑一遍却对不上。分工是：有单的存量 9 人从探针读（本次没动他们的单），
    其余 111 人（31 补单 + 80 新增）从 plans 读。
    """
    problems = []
    with_orders = [u for u in b["users"] if u["nall"] > 0]

    freq = Counter(frequency_of(u["n30"]) for u in with_orders)
    pref = Counter(pref_of(u["nf"], u["ns"]) for u in with_orders)
    for p in plans:
        freq[frequency_of(p.n30)] += 1
        pref[pref_of(p.nf, p.ns)] += 1
    for k, want in TARGET_FREQ.items():
        if freq.get(k, 0) != want:
            problems.append(f"频率 {k}: {freq.get(k,0)} ≠ 目标 {want}")
    for k, want in TARGET_PREF.items():
        if pref.get(k, 0) != want:
            problems.append(f"快慢充 {k}: {pref.get(k,0)} ≠ 目标 {want}")

    # 价值：对全体按近30天金额降序排名，就是 ROW_NUMBER() 的读法
    rows = [(u["amt30"], u["id"]) for u in with_orders]
    rows += [(p.amt30, 10 ** 9 + i) for i, p in enumerate(plans)]
    if len(rows) != TOTAL_USERS:
        problems.append(f"参与价值排名的人 {len(rows)} ≠ {TOTAL_USERS}")
    rows.sort(key=lambda r: (-r[0], r[1]))
    lvl = Counter()
    for rank in range(1, len(rows) + 1):
        lvl["high" if rank <= 24 else "medium" if rank <= 84 else "low"] += 1
    for k, want in TARGET_VALUE.items():
        if lvl.get(k, 0) != want:
            problems.append(f"价值 {k}: {lvl.get(k,0)} ≠ 目标 {want}")

    amounts = [a for a, _ in rows]
    for bd in (24, 84):
        if amounts[bd - 1] == amounts[bd]:
            problems.append(f"第 {bd}/{bd+1} 名金额并列（{amounts[bd-1]}），"
                            f"分位边界不干净；换个人算就换一套分档")
    # 文件头那两条空隙，实测复核
    if amounts[23] < BAND["H"][0]:
        problems.append(f"第 24 名 {amounts[23]} < band H 下界 {BAND['H'][0]}")
    if amounts[83] < BAND_M_FLOOR:
        problems.append(f"第 84 名 {amounts[83]} < band M 下界 {BAND_M_FLOOR}")

    for p in plans:
        if p.band and p.n30:
            lo, hi = BAND[p.band]
            if not (lo - BAND_TOL <= p.amt30 <= hi + BAND_TOL):
                problems.append(f"{p.phone} 近30天金额 {p.amt30} 跑出 band {p.band}")
        if p.order_days and max(p.order_days) >= p.created_days:
            problems.append(f"{p.phone} 有订单早于注册"
                            f"（{max(p.order_days)} ≥ {p.created_days} 天前）")
    for r in user_rows:
        if r["active_days"] >= r["created_days"]:
            problems.append(f"{r['phone']} 最近活跃({r['active_days']}) 不早于注册"
                            f"({r['created_days']})")
    return problems


# ---------------------------------------------------------------------------
# 5. 产出
# ---------------------------------------------------------------------------

HEADER = """-- 用户群体管理大屏 —— 演示数据（docs/数据.md 二）
-- 由 sql/gen_user_groups_demo.py 生成，**不要手改**，改生成器再重跑。
--
-- 用法：
--   mysql -u charging_user -p123456 charging_system < sql/seed_user_groups_demo.sql
--
-- 幂等：连跑两次，第二次不新增任何行（靠 phone / order_no 的 NOT EXISTS）。
-- 刻意不用 INSERT IGNORE —— 它会把外键错 1452、ENUM 非法值、CHECK 违反
-- 全部降级成 warning 静默跳过，少插几行而脚本报成功。
--
-- 但幂等只保证「不重复插入」，**不保证「把改坏的行修回来」**：
-- 存量 40 人的六列属性是 UPDATE（每次重写），新增 80 人是 INSERT ... WHERE NOT EXISTS
-- （已存在就整行跳过）。所以要是谁把新增用户的 city 改成空串，重跑本文件修不好，
-- 得走下面的「刷新」两行（reset 删掉 80 人再重新插入）。
--
-- 刷新：本文件的时间全部相对 @anchor = NOW()，所以**首次执行时**新鲜。
-- 放一个月，近30天的单会滑出窗口，沉默用户会冲破 30% 上限。要刷新就跑：
--   mysql ... < sql/reset_user_groups_demo.sql
--   mysql ... < sql/seed_user_groups_demo.sql
--
-- 统计口径（docs/数据.md 三）：画像/行为/趋势/价值只用 charge_order.status='settled'。
-- 近30天统一 `end_time >= CURDATE() - INTERVAL 30 DAY`（与 server/database.cpp 同款）。
-- 本批**不含 29/30 天前的已结算单** —— CURDATE() 与 NOW() 两种口径在边界日上结果
-- 不同，压在边界上占比会抖；那两个日子只放非 settled 状态。
--
-- 价值分层是对**全体 120 人**按近30天金额取名次（0 消费并列垫底），
-- 不是只对有消费的 92 人取分位。用值域阈值（金额 ≤ 30 分位）算会得到低价值 28 人
-- 而不是 36 人，三档全飞出要求。
--
-- 不碰：13800138001 的余额与现有订单、任何现有 charge_order 行、
-- wallet_transactions、pile.current_user_id。

USE charging_system;

-- 全脚本唯一的基准时刻。不用它的话，脚本跨零点执行时前面几行和后面几行
-- 会是不同的「今天」，压在边界上的行会飘。
SET @anchor = NOW();

-- ⚠️⚠️ 想给「N 天前那一天」配一个固定钟点，**绝不能写
--     TIMESTAMP(DATE_ADD(@anchor, INTERVAL -N DAY), '10:00:00')**。
-- MySQL 的 TIMESTAMP(expr1, expr2) 是把 expr2 当作**时间间隔加到** expr1 上，
-- 不是「把 expr1 的时分秒换成 expr2」。上面那行等于「N 天前 + 10 小时」，
-- 于是：① anchor 小时 ≥ 14 时日期会跨到后一天；② 时分秒变成 anchor 的
-- 时分秒 +10h，每次重跑都不一样；③ 订单的「凌晨/上午/下午/晚间」四段
-- 会被整体旋转 anchor 小时，时段分析全错。
-- 正确写法是把日期和钟点拼起来再转 DATETIME：
--     CAST(CONCAT(DATE(DATE_ADD(@anchor, INTERVAL -N DAY)), ' 10:00:00') AS DATETIME)
-- 下面所有固定钟点的地方都照这个写；3.5 有一条自检专门守这个坑。

-- ===== 0. 执行前基线自检：不符合预期就先别往下跑 =====
SELECT '执行前' AS stage,
       (SELECT COUNT(*) FROM `user`)                                        AS 用户数,
       (SELECT COUNT(*) FROM `user` WHERE phone LIKE '1370000%')             AS 本批用户,
       (SELECT COUNT(*) FROM charge_order)                                   AS 订单数,
       (SELECT COUNT(*) FROM charge_order WHERE order_no LIKE BINARY 'UG%')  AS 本批订单;
-- 期望 40 / 0 / {BASE_ORDERS} / 0。不是就说明库被动过，配额要重推。

-- ===== 1. 本批用户（存量 40 人补属性 + 新增 {N_NEW} 人） =====
-- 临时表必须显式写 COLLATE=utf8mb4_unicode_ci：MySQL 8 的默认排序规则是
-- utf8mb4_0900_ai_ci，和本库各表的 utf8mb4_unicode_ci 不同，拿 phone 去
-- JOIN `user` 会直接报 ERROR 1267（Illegal mix of collations）。
DROP TEMPORARY TABLE IF EXISTS seed_ug_user;
CREATE TEMPORARY TABLE seed_ug_user (
    phone               VARCHAR(11)   NOT NULL,
    is_new              TINYINT       NOT NULL,
    nickname            VARCHAR(64)   NOT NULL DEFAULT '',
    gender              VARCHAR(16)   NOT NULL DEFAULT 'unknown',
    age_group           VARCHAR(16)   NOT NULL DEFAULT 'unknown',
    city                VARCHAR(64)   NOT NULL DEFAULT '',
    vehicle_type        VARCHAR(16)   NOT NULL DEFAULT 'unknown',
    registration_source VARCHAR(16)   NOT NULL DEFAULT 'unknown',
    balance             DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    status              VARCHAR(16)   NOT NULL DEFAULT 'normal',
    created_days        INT           NOT NULL,
    active_days         INT           NOT NULL,
    PRIMARY KEY (phone)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO seed_ug_user
    (phone, is_new, nickname, gender, age_group, city, vehicle_type,
     registration_source, balance, status, created_days, active_days)
VALUES
{{USER_ROWS}}

-- 存量 40 人：只补六列属性 + 注册时间 + 最近活跃时间。
-- 余额、状态、昵称、现有订单一行都不动。
UPDATE `user` u JOIN seed_ug_user s ON s.phone = u.phone
SET u.gender              = s.gender,
    u.age_group           = s.age_group,
    u.city                = s.city,
    u.vehicle_type        = s.vehicle_type,
    u.registration_source = s.registration_source,
    u.created_at          = CAST(CONCAT(DATE(DATE_ADD(@anchor, INTERVAL -s.created_days DAY)),
                                        ' 10:00:00') AS DATETIME),
    u.last_active_at      = CAST(CONCAT(DATE(DATE_ADD(@anchor, INTERVAL -s.active_days DAY)),
                                        ' ',
                                        LPAD(s.active_days * 7 % 24, 2, '0'), ':30:00') AS DATETIME)
WHERE s.is_new = 0;

-- 新增 {N_NEW} 人。LEFT JOIN + IS NULL 保证重复执行不重复插入。
INSERT INTO `user`
    (phone, nickname, gender, age_group, city, vehicle_type,
     registration_source, balance, status, created_at, last_active_at)
SELECT s.phone, s.nickname, s.gender, s.age_group, s.city, s.vehicle_type,
       s.registration_source, s.balance, s.status,
       CAST(CONCAT(DATE(DATE_ADD(@anchor, INTERVAL -s.created_days DAY)),
                   ' 10:00:00') AS DATETIME),
       CAST(CONCAT(DATE(DATE_ADD(@anchor, INTERVAL -s.active_days DAY)),
                   ' ',
                   LPAD(s.active_days * 7 % 24, 2, '0'), ':30:00') AS DATETIME)
FROM seed_ug_user s
LEFT JOIN `user` u ON u.phone = s.phone
WHERE s.is_new = 1 AND u.id IS NULL;

-- ===== 2. 本批订单 =====
-- 只存「相对量」：天数偏移、时刻字符串、时长、间隔。绝对时间在下面的
-- INSERT ... SELECT 里用 @anchor 一次性算出来，这样隔多久执行都是新鲜的。
DROP TEMPORARY TABLE IF EXISTS seed_ug_order;
CREATE TEMPORARY TABLE seed_ug_order (
    seq          INT           NOT NULL,
    order_no     VARCHAR(32)   NOT NULL,
    phone        VARCHAR(11)   NOT NULL,
    pile_id      BIGINT        NOT NULL,
    status       VARCHAR(20)   NOT NULL,
    kwh          DECIMAL(10,2) NOT NULL DEFAULT 0.00,
    start_days   INT           NOT NULL,
    start_time_s CHAR(8)       NOT NULL,
    dur_s        INT           NOT NULL DEFAULT 0,
    gap_s        INT           NOT NULL DEFAULT 0,
    anchor_min   INT           NULL,
    pay_no       VARCHAR(64)   NULL,
    PRIMARY KEY (seq),
    UNIQUE KEY uk_seed_order_no (order_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO seed_ug_order
    (seq, order_no, phone, pile_id, status, kwh,
     start_days, start_time_s, dur_s, gap_s, anchor_min, pay_no)
VALUES
{{ORDER_ROWS}}

-- station_id 与 unit_price 一律从 pile JOIN station **推导**，绝不来自本表 ——
-- 外键只保证「站存在」，不保证 order.station_id = pile.station_id，
-- 也不保证单价是下单时的站点价。
-- 空值严格按状态语义（照存量约定，不是脏数据）：
--   cancelled / reserved   reserve_time 有，start/end/pay 为 NULL，电量金额 0
--   charging               start 有，end/pay 为 NULL
--   pending_payment        start/end 有，pay 为 NULL
--   settled                字段齐全且 pay_request_id 非空
-- created_at = reserve_time，与存量 337 笔一致。
--
-- 开始时刻有两种算法（见生成器里的 TS）：
--   anchor_min 为 NULL   N 天前的某个时刻，绝大多数单走这条
--   anchor_min 非空      「多少分钟前」，只给 charging/reserved 这类进行中的单 ——
--                        天偏移最小是 1，会排成「充了 28 天的电」；用 0 又落在
--                        今天、可能越过 @anchor 变成未来时间
-- 开始时刻在派生表里只算一次（MySQL 不允许在同层 SELECT 里引用刚起好的别名），
-- 免得同一个 CASE 在四个时间列里抄四遍。
INSERT INTO charge_order
    (order_no, user_id, station_id, pile_id, status, unit_price,
     reserve_time, start_time, end_time, duration_seconds,
     kwh, amount, pay_request_id, created_at)
SELECT o.order_no, u.id, p.station_id, p.id, o.status, st.price,
       o.start_at - INTERVAL o.gap_s SECOND,
       CASE WHEN o.status IN ('cancelled','reserved') THEN NULL
            ELSE o.start_at END,
       CASE WHEN o.status IN ('settled','pending_payment')
            THEN o.start_at + INTERVAL o.dur_s SECOND
            ELSE NULL END,
       CASE WHEN o.status IN ('cancelled','reserved') THEN 0 ELSE o.dur_s END,
       o.kwh, ROUND(o.kwh * st.price, 2), o.pay_no,
       o.start_at - INTERVAL o.gap_s SECOND
FROM (SELECT o.*,
             CASE WHEN o.anchor_min IS NOT NULL
                  THEN DATE_ADD(@anchor, INTERVAL -o.anchor_min MINUTE)
                  ELSE CAST(CONCAT(DATE(DATE_ADD(@anchor, INTERVAL -o.start_days DAY)),
                                   ' ', o.start_time_s) AS DATETIME)
             END AS start_at
      FROM seed_ug_order o) o
JOIN `user` u   ON u.phone = o.phone
JOIN pile p     ON p.id = o.pile_id
JOIN station st ON st.id = p.station_id
LEFT JOIN charge_order x ON x.order_no = o.order_no
WHERE x.id IS NULL;

-- ===== 3. 自检 =====
-- ⚠️ MySQL 的**临时表在同一条语句里只能被打开一次**（ERROR 1137 Can't reopen
--    table），所以下面凡是多处要用同一个统计量的地方，一律先 SET 进会话变量
--    再引用 —— 每个 SET 是独立语句，可以各开一次。不要改回 UNION ALL 里
--    反复 FROM 同一张临时表。
--
-- 3.1 各用户的近30天已结算单数与金额（口径与 docs/数据.md 三 一致）
DROP TEMPORARY TABLE IF EXISTS seed_ug_n30;
CREATE TEMPORARY TABLE seed_ug_n30 (
    user_id BIGINT PRIMARY KEY, n30 INT NOT NULL, amt30 DECIMAL(12,2) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO seed_ug_n30
SELECT u.id, COALESCE(d.c, 0), COALESCE(d.s, 0)
FROM `user` u
LEFT JOIN (SELECT user_id, COUNT(*) c, SUM(amount) s FROM charge_order
           WHERE status = 'settled'
             AND end_time >= CURDATE() - INTERVAL 30 DAY
           GROUP BY user_id) d ON d.user_id = u.id;

-- 3.2 行数与基线
SET @n_batch_user = (SELECT COUNT(*) FROM `user` WHERE phone LIKE '1370000%');
SET @n_all_user   = (SELECT COUNT(*) FROM `user`);
SET @n_batch_ord  = (SELECT COUNT(*) FROM charge_order WHERE order_no LIKE BINARY 'UG%');
SET @n_settled    = (SELECT COUNT(*) FROM charge_order
                     WHERE order_no LIKE BINARY 'UG%' AND status = 'settled');
SET @n_cancelled  = (SELECT COUNT(*) FROM charge_order
                     WHERE order_no LIKE BINARY 'UG%' AND status = 'cancelled');
SET @n_pos        = (SELECT COUNT(*) FROM seed_ug_n30 WHERE amt30 > 0);
SELECT '本批新增用户' AS 检查项, @n_batch_user AS 实测, {N_NEW} AS 期望,
       IF(@n_batch_user = {N_NEW}, 'OK', '*** 不符 ***') AS 结果
UNION ALL SELECT '全库用户总数',   @n_all_user,   {N_ALL},       IF(@n_all_user = {N_ALL}, 'OK', '*** 不符 ***')
UNION ALL SELECT '本批订单数',     @n_batch_ord,  {N_ORDERS},    IF(@n_batch_ord = {N_ORDERS}, 'OK', '*** 不符 ***')
UNION ALL SELECT '本批已结算',     @n_settled,    {N_SETTLED},   IF(@n_settled = {N_SETTLED}, 'OK', '*** 不符 ***')
UNION ALL SELECT '本批 cancelled', @n_cancelled,  {N_CANCELLED}, IF(@n_cancelled = {N_CANCELLED}, 'OK', '*** 不符 ***')
UNION ALL SELECT '有消费的用户',   @n_pos,        92,            IF(@n_pos = 92, 'OK', '*** 不符 ***')
UNION ALL SELECT '零消费的用户',   @n_all_user - @n_pos, 28,     IF(@n_all_user - @n_pos = 28, 'OK', '*** 不符 ***');

-- 3.3 频率与快慢充比例
DROP TEMPORARY TABLE IF EXISTS seed_ug_pref;
CREATE TEMPORARY TABLE seed_ug_pref (
    user_id BIGINT PRIMARY KEY, nf INT NOT NULL, ns INT NOT NULL, pref VARCHAR(8) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO seed_ug_pref
SELECT user_id, nf, ns,
       CASE WHEN nf + ns = 0          THEN 'mixed'
            WHEN nf / (nf + ns) >= 0.6 THEN 'fast'
            WHEN nf / (nf + ns) <= 0.4 THEN 'slow'
            ELSE 'mixed' END
FROM (SELECT u.id AS user_id, COALESCE(t.nf, 0) AS nf, COALESCE(t.ns, 0) AS ns
      FROM `user` u
      LEFT JOIN (SELECT o.user_id, SUM(p.type = 'fast') nf, SUM(p.type = 'slow') ns
                 FROM charge_order o JOIN pile p ON p.id = o.pile_id
                 WHERE o.status = 'settled' GROUP BY o.user_id) t
             ON t.user_id = u.id) x;

SET @f_high  = (SELECT COUNT(*) FROM seed_ug_n30 WHERE n30 >= 8);
SET @f_med   = (SELECT COUNT(*) FROM seed_ug_n30 WHERE n30 BETWEEN 3 AND 7);
SET @f_low   = (SELECT COUNT(*) FROM seed_ug_n30 WHERE n30 BETWEEN 1 AND 2);
SET @f_ina   = (SELECT COUNT(*) FROM seed_ug_n30 WHERE n30 = 0);
SET @p_fast  = (SELECT COUNT(*) FROM seed_ug_pref WHERE pref = 'fast');
SET @p_slow  = (SELECT COUNT(*) FROM seed_ug_pref WHERE pref = 'slow');
SET @p_mixed = (SELECT COUNT(*) FROM seed_ug_pref WHERE pref = 'mixed');
SELECT '频率 high' AS 分组, @f_high AS 人数, @n_all_user AS 总数,
       ROUND(@f_high * 100.0 / @n_all_user, 1) AS 占比, '10~15%' AS 要求,
       IF(@f_high * 100.0 / @n_all_user BETWEEN 10 AND 15, 'OK', '*** 超范围 ***') AS 结果
UNION ALL SELECT '频率 medium', @f_med, @n_all_user,
       ROUND(@f_med * 100.0 / @n_all_user, 1), '25~30%',
       IF(@f_med * 100.0 / @n_all_user BETWEEN 25 AND 30, 'OK', '*** 超范围 ***')
UNION ALL SELECT '频率 low', @f_low, @n_all_user,
       ROUND(@f_low * 100.0 / @n_all_user, 1), '30~35%',
       IF(@f_low * 100.0 / @n_all_user BETWEEN 30 AND 35, 'OK', '*** 超范围 ***')
UNION ALL SELECT '频率 inactive', @f_ina, @n_all_user,
       ROUND(@f_ina * 100.0 / @n_all_user, 1), '20~30%',
       IF(@f_ina * 100.0 / @n_all_user BETWEEN 20 AND 30, 'OK', '*** 超范围 ***')
UNION ALL SELECT '快慢充 fast', @p_fast, @n_all_user,
       ROUND(@p_fast * 100.0 / @n_all_user, 1), '35~45%',
       IF(@p_fast * 100.0 / @n_all_user BETWEEN 35 AND 45, 'OK', '*** 超范围 ***')
UNION ALL SELECT '快慢充 slow', @p_slow, @n_all_user,
       ROUND(@p_slow * 100.0 / @n_all_user, 1), '20~30%',
       IF(@p_slow * 100.0 / @n_all_user BETWEEN 20 AND 30, 'OK', '*** 超范围 ***')
UNION ALL SELECT '快慢充 mixed', @p_mixed, @n_all_user,
       ROUND(@p_mixed * 100.0 / @n_all_user, 1), '25~35%',
       IF(@p_mixed * 100.0 / @n_all_user BETWEEN 25 AND 35, 'OK', '*** 超范围 ***');

-- 3.4 价值分层：对全体 {N_ALL} 人按近30天金额取名次（0 消费并列垫底）
DROP TEMPORARY TABLE IF EXISTS seed_ug_value;
CREATE TEMPORARY TABLE seed_ug_value (
    user_id BIGINT PRIMARY KEY, rk INT NOT NULL, amt30 DECIMAL(12,2) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO seed_ug_value
SELECT user_id, ROW_NUMBER() OVER (ORDER BY amt30 DESC, user_id), amt30
FROM seed_ug_n30;
SET @v_high = (SELECT COUNT(*) FROM seed_ug_value WHERE rk <= 24);
SET @v_med  = (SELECT COUNT(*) FROM seed_ug_value WHERE rk BETWEEN 25 AND 84);
SET @v_low  = (SELECT COUNT(*) FROM seed_ug_value WHERE rk >= 85);
SELECT '价值 high' AS 分组, @v_high AS 人数, @n_all_user AS 总数,
       ROUND(@v_high * 100.0 / @n_all_user, 1) AS 占比, '20%' AS 要求
UNION ALL SELECT '价值 medium', @v_med, @n_all_user,
       ROUND(@v_med * 100.0 / @n_all_user, 1), '50%'
UNION ALL SELECT '价值 low', @v_low, @n_all_user,
       ROUND(@v_low * 100.0 / @n_all_user, 1), '30%';

-- 名次边界必须干净（第 24/25、84/85 名金额不能相等），
-- 否则换个人算（RANK 还是 ROW_NUMBER、并列怎么破）就换一套分档。
SET @a24 = (SELECT amt30 FROM seed_ug_value WHERE rk = 24);
SET @a25 = (SELECT amt30 FROM seed_ug_value WHERE rk = 25);
SET @a84 = (SELECT amt30 FROM seed_ug_value WHERE rk = 84);
SET @a85 = (SELECT amt30 FROM seed_ug_value WHERE rk = 85);
SELECT '第24/25名' AS 分位边界, @a24 AS 上一名金额, @a25 AS 下一名金额,
       IF(@a24 > @a25, 'OK', '*** 并列，分档不干净 ***') AS 结果
UNION ALL SELECT '第84/85名', @a84, @a85,
       IF(@a84 > @a85, 'OK', '*** 并列，分档不干净 ***');
-- 期望 第24名 > 45.00（band H 下界 80 以上）、第84名 > 7.50（band M 下界 8 以上）
-- —— 中间那段是新数据的专属名次区间，存量金额一个都不许插进来。

-- 3.5 脏数据：全部必须为 0
DROP TEMPORARY TABLE IF EXISTS seed_ug_dirty;
CREATE TEMPORARY TABLE seed_ug_dirty (item VARCHAR(48), n BIGINT);
INSERT INTO seed_ug_dirty
SELECT '时间倒置(end<start)', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND end_time IS NOT NULL AND end_time < start_time
UNION ALL SELECT '未来时间(end>NOW)', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND end_time > NOW()
UNION ALL SELECT '未来时间(start>NOW)', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND start_time > NOW()
UNION ALL SELECT '未来时间(reserve>NOW)', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND reserve_time > NOW()
UNION ALL SELECT '负电量', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND kwh < 0
UNION ALL SELECT '负金额', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND amount < 0
UNION ALL SELECT '金额≠电量×单价', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status = 'settled'
    AND ABS(amount - ROUND(kwh * unit_price, 2)) > 0.001
UNION ALL SELECT '时长与起止不符', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status = 'settled'
    AND ABS(duration_seconds - TIMESTAMPDIFF(SECOND, start_time, end_time)) > 1
UNION ALL SELECT '已结算缺支付号', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status = 'settled' AND pay_request_id IS NULL
UNION ALL SELECT '取消/预约单有电量', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status IN ('cancelled','reserved')
    AND (kwh <> 0 OR amount <> 0)
UNION ALL SELECT '取消/预约单有起止时间', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status IN ('cancelled','reserved')
    AND (start_time IS NOT NULL OR end_time IS NOT NULL)
UNION ALL SELECT '已结算/待支付缺起止时间', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status IN ('settled','pending_payment')
    AND (start_time IS NULL OR end_time IS NULL)
UNION ALL SELECT '站点与电桩不匹配', COUNT(*) FROM charge_order o
  JOIN pile p ON p.id = o.pile_id
  WHERE o.order_no LIKE BINARY 'UG%' AND o.station_id <> p.station_id
UNION ALL SELECT '外键失效(用户)', COUNT(*) FROM charge_order o
  LEFT JOIN `user` u ON u.id = o.user_id
  WHERE o.order_no LIKE BINARY 'UG%' AND u.id IS NULL
UNION ALL SELECT '外键失效(电桩)', COUNT(*) FROM charge_order o
  LEFT JOIN pile p ON p.id = o.pile_id
  WHERE o.order_no LIKE BINARY 'UG%' AND p.id IS NULL
UNION ALL SELECT '外键失效(站点)', COUNT(*) FROM charge_order o
  LEFT JOIN station s ON s.id = o.station_id
  WHERE o.order_no LIKE BINARY 'UG%' AND s.id IS NULL
UNION ALL SELECT '订单早于用户注册', COUNT(*) FROM charge_order o
  JOIN `user` u ON u.id = o.user_id
  WHERE o.order_no LIKE BINARY 'UG%' AND o.reserve_time < u.created_at
UNION ALL SELECT '落在非真站(73)', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND station_id = 73
UNION ALL SELECT '进行中的单太旧(>12h)', COUNT(*) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%' AND status IN ('charging','reserved')
    AND reserve_time < NOW() - INTERVAL 12 HOUR
UNION ALL SELECT '充电动量与已充时长不符', COUNT(*) FROM charge_order o
  JOIN pile p ON p.id = o.pile_id
  WHERE o.order_no LIKE BINARY 'UG%' AND o.status = 'charging'
    AND ABS(o.duration_seconds - TIMESTAMPDIFF(SECOND, o.start_time, NOW())) > 120
UNION ALL SELECT '本批订单号重复', COUNT(*) - COUNT(DISTINCT order_no) FROM charge_order
  WHERE order_no LIKE BINARY 'UG%'
UNION ALL SELECT '本批手机号重复', COUNT(*) - COUNT(DISTINCT phone) FROM `user`
  WHERE phone LIKE '1370000%'
-- 开始时刻必须**正好等于**模板钟点。历史上这里用过
-- TIMESTAMP(date, 'HH:MM:SS')，那玩意儿是把间隔加上去，会把
-- 凌晨/上午/下午/晚间四段整体旋转 anchor 小时，时段分析全错。
-- anchor_min 非空的是 charging/reserved 这类「多少分钟前」的单，
-- 它们本来就不走模板钟点，排除掉。
UNION ALL SELECT '开始时刻≠模板钟点', COUNT(*) FROM charge_order o
  JOIN seed_ug_order g ON g.order_no = o.order_no
  WHERE o.order_no LIKE BINARY 'UG%' AND g.start_time_s IS NOT NULL
    AND g.anchor_min IS NULL
    AND TIME(o.start_time) <> g.start_time_s
-- 存量用户的注册钟点也必须是写死的 10:00:00（新增的同样）
UNION ALL SELECT '注册钟点≠10:00:00', COUNT(*) FROM `user`
  WHERE TIME(created_at) <> '10:00:00'
    AND (phone LIKE '1370000%' OR phone IN (SELECT phone FROM seed_ug_user WHERE is_new = 0));
SELECT item AS 检查项, n AS 命中, IF(n = 0, 'OK', '*** 有脏数据 ***') AS 结果
FROM seed_ug_dirty;

-- 3.6 演示账号未被改动（本脚本一个字都没碰它；
--     数字对不上多半是有人演示时自己充过值，不是本脚本干的）
SELECT u.phone AS 账号, u.balance AS 余额, u.status AS 状态,
       (SELECT COUNT(*) FROM charge_order WHERE user_id = u.id) AS 订单数,
       (SELECT COUNT(*) FROM charge_order o
        WHERE o.user_id = u.id AND o.order_no LIKE BINARY 'UG%') AS 本批订单
FROM `user` u WHERE u.phone = '13800138001';
-- 生成时的实测值：余额 {DEMO_BALANCE}、订单 {DEMO_ORDERS} 笔、本批订单 0 笔。

-- 3.7 结论：3.2~3.5 的判定全过**且**脏数据项数为 0 才算通过。
-- 早先这条只看前面那 15 项，脏数据有命中时照样打 ✅，很容易看漏。
SET @n_dirty = (SELECT COUNT(*) FROM seed_ug_dirty WHERE n <> 0);
SELECT IF(
    (@n_batch_user = {N_NEW})
  + (@n_all_user   = {N_ALL})
  + (@n_batch_ord  = {N_ORDERS})
  + (@n_settled    = {N_SETTLED})
  + (@n_cancelled  = {N_CANCELLED})
  + (@n_pos        = 92)
  + (@f_high  * 100.0 / @n_all_user BETWEEN 10 AND 15)
  + (@f_med   * 100.0 / @n_all_user BETWEEN 25 AND 30)
  + (@f_low   * 100.0 / @n_all_user BETWEEN 30 AND 35)
  + (@f_ina   * 100.0 / @n_all_user BETWEEN 20 AND 30)
  + (@p_fast  * 100.0 / @n_all_user BETWEEN 35 AND 45)
  + (@p_slow  * 100.0 / @n_all_user BETWEEN 20 AND 30)
  + (@p_mixed * 100.0 / @n_all_user BETWEEN 25 AND 35)
  + (@a24 > @a25)
  + (@a84 > @a85) = 15
  AND @n_dirty = 0,
  '✅ 行数、比例、名次边界、脏数据自检全部通过',
  '❌ 有项目不符，请回看上面的表') AS 结论,
  @n_dirty AS 脏数据项数;

DROP TEMPORARY TABLE IF EXISTS seed_ug_user;
DROP TEMPORARY TABLE IF EXISTS seed_ug_order;
DROP TEMPORARY TABLE IF EXISTS seed_ug_n30;
DROP TEMPORARY TABLE IF EXISTS seed_ug_pref;
DROP TEMPORARY TABLE IF EXISTS seed_ug_value;
DROP TEMPORARY TABLE IF EXISTS seed_ug_dirty;
"""


def plan_row(p):
    """把 Plan 转成 seed_ug_user 的一行。存量 40 人由 plan_existing 直接给，
    新增 80 人走这里 —— 两边字段必须齐，否则 INSERT 的列数会对不上。"""
    return {
        "phone": p.phone, "is_new": True, "nickname": p.nickname,
        "gender": p.gender, "age_group": p.age_group, "city": p.city,
        "vehicle_type": p.vehicle_type, "registration_source": p.registration_source,
        "balance": p.balance, "status": p.status,
        "created_days": p.created_days, "active_days": p.active_days,
    }


def _lit(s):
    return "'" + str(s).replace("\\", "\\\\").replace("'", "''") + "'"


def sql_text(user_rows, orders, b, stats):
    rows = []
    for r in sorted(user_rows, key=lambda x: (x["is_new"], x["phone"])):
        rows.append(f"({_lit(r['phone'])},{1 if r['is_new'] else 0},"
                    f"{_lit(r['nickname'])},"
                    f"'{r['gender']}','{r['age_group']}',{_lit(r['city'])},"
                    f"'{r['vehicle_type']}','{r['registration_source']}',"
                    f"{r['balance']},'{r['status']}',"
                    f"{r['created_days']},{r['active_days']})")
    user_block = ",\n".join(rows) + ";"

    orows = []
    for o in orders:
        pay = f"'{o['pay_no']}'" if o["pay_no"] else "NULL"
        amin = o["anchor_min"] if o["anchor_min"] is not None else "NULL"
        orows.append(f"({o['seq']},'{o['order_no']}','{o['phone']}',{o['pile_id']},"
                     f"'{o['status']}',{o['kwh']},{o['start_days']},"
                     f"'{o['start_time_s']}',{o['dur_s']},{o['gap_s']},{amin},{pay})")
    chunks = [",\n".join(orows[i:i + 100]) + ";" for i in range(0, len(orows), 100)]
    order_block = "\n\nINSERT INTO seed_ug_order VALUES\n".join(chunks)

    return (HEADER
            .replace("{{USER_ROWS}}", user_block)
            .replace("{{ORDER_ROWS}}", order_block)
            .replace("{BASE_ORDERS}", str(b["order_count"]))
            .replace("{N_NEW}", str(stats["n_new"]))
            .replace("{N_ALL}", str(TOTAL_USERS))
            .replace("{N_ORDERS}", str(stats["n_orders"]))
            .replace("{N_SETTLED}", str(stats["n_settled"]))
            .replace("{N_CANCELLED}", str(stats["n_cancelled"]))
            .replace("{DEMO_BALANCE}", f"{b['demo_balance']:.2f}")
            .replace("{DEMO_ORDERS}", str(b["demo"]["nall"])))


def main():
    ap = argparse.ArgumentParser(description="生成用户群体大屏的演示数据种子")
    ap.add_argument("--dry-run", action="store_true", help="配平 + 自检，不写文件")
    args = ap.parse_args()

    rng = random.Random(SEED)
    conn = common.connect()
    try:
        b = probe(conn)
        with conn.cursor() as cur:
            cur.execute("SELECT balance FROM `user` WHERE phone = '13800138001'")
            row = cur.fetchone()
            b["demo_balance"] = q2(row[0]) if row else Decimal("0.00")
    finally:
        conn.close()

    print("=" * 74)
    print(f"库 {b['db']}  MySQL {b['server']}  今天 {b['today']}")
    print(f"存量 {b['user_count']} 人 / {b['order_count']} 单 / "
          f"站点 {len(b['stations'])} 座 / 电桩 {len(b['piles'])} 根")
    print("=" * 74)

    problems = assert_baseline(b)
    if problems:
        print("\n❌ 基线不符合预期，未生成文件：", file=sys.stderr)
        for x in problems:
            print(f"   - {x}", file=sys.stderr)
        return 1

    user_rows, old_plans, promoted = plan_existing(b, rng)
    new_plans = plan_new(b, rng)

    # 快慢充名额从「目标总数 − 存量 9 个已有单的人」倒推，再分两批派下去。
    # 先派受限的存量 31 人（n 只有 1~2），再派新增 80 人（n 大得多，好配）。
    need = Counter(TARGET_PREF)
    for u in b["users"]:
        if u["nall"] > 0:
            need[u["pref"]] -= 1             # 存量 9 人全是 fast，先占掉名额
    need = assign_prefs(old_plans, need, rng)
    need = assign_prefs(new_plans, need, rng)
    if any(v != 0 for v in need.values()):
        print(f"\n❌ 快慢充名额没配平，还剩 {dict(need)}", file=sys.stderr)
        return 1
    print(f"存量 40 人：补六列属性 + 铺开注册时间；"
          f"{len(promoted)} 人补近30天的单（顶出 inactive），"
          f"{len(old_plans) - len(promoted)} 人补 31~88 天的单")
    plans = old_plans + new_plans
    assign_non_settled(plans, rng)
    orders = plan_orders(plans, b, rng)

    # 最近活跃时间 = 最近一单，排完才知道，回填到用户行上
    plan_by_phone = {p.phone: p for p in plans}
    for r in user_rows:
        p = plan_by_phone.get(r["phone"])
        if p is not None:
            r["active_days"] = p.active_days
    user_rows += [plan_row(p) for p in new_plans]     # 新增的 80 人补进用户行
    if len(user_rows) != TOTAL_USERS:
        print(f"\n❌ 用户行 {len(user_rows)} ≠ {TOTAL_USERS}", file=sys.stderr)
        return 1
    print(f"新增 {len(new_plans)} 人 -> 全库 {len(user_rows)} 人")

    buckets = Counter(o["status"] for o in orders)
    n = len(orders)
    print(f"新增订单 {n} 笔 -> 全库 {b['order_count'] + n} 笔")
    print(f"  本批口径 已结算 {buckets['settled']/n:.1%} / "
          f"cancelled {buckets['cancelled']/n:.1%} / "
          f"其他 {(n - buckets['settled'] - buckets['cancelled'])/n:.1%}"
          f"（要求 80 / 5~10 / 10）")

    print("\n自检（按 SQL 同款口径对全体 120 人重算）：")
    problems = verify(user_rows, plans, b)
    if problems:
        print("❌ 未通过，未写文件：", file=sys.stderr)
        for x in problems:
            print(f"   - {x}", file=sys.stderr)
        return 1

    freq = Counter(frequency_of(u["n30"]) for u in b["users"] if u["nall"] > 0)
    pref = Counter(pref_of(u["nf"], u["ns"]) for u in b["users"] if u["nall"] > 0)
    for p in plans:
        freq[frequency_of(p.n30)] += 1
        pref[pref_of(p.nf, p.ns)] += 1
    print(f"  ✅ 频率    {dict(freq)}")
    print(f"  ✅ 快慢充  {dict(pref)}")
    print("  ✅ 价值 24/60/36，名次边界干净，两处金额带空隙实测成立")

    if args.dry_run:
        print("\n（--dry-run，未写文件）")
        return 0

    stats = {"n_new": len(new_plans), "n_orders": n,
             "n_settled": buckets["settled"], "n_cancelled": buckets["cancelled"]}
    text = sql_text(user_rows, orders, b, stats)
    OUT_PATH.write_text(text, encoding="utf-8")
    print(f"\n✅ 已写出 {OUT_PATH}（{len(text)} 字节，"
          f"{len(user_rows)} 用户行 + {n} 订单行）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
