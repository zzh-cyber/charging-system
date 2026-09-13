"""模拟充电订单生成 + 场景质量问题注入（NO.109 / NO.110）。

纯生成逻辑：不连数据库、不写文件。输入维表行 + 参数，输出订单行与
「注入数 / 期望探查数」两份计数。这样 9/14 调参只要改参数，不用动代码。

口径要点（改之前先读，这几条是踩过的坑）：

1. kwh 是【权威值】，时长由它反推
       kwh        = (target_soc - start_soc)/100 * battery_capacity
       duration_h = kwh / (power_kw * 0.92)
   反过来先定时长再算 kwh 会造出 180kW * 1.5h * 0.92 = 248 度这种
   物理上不存在的行 —— 电池才 40~80 度、SOC 只涨一两格。

2. 每个脏行【只注入一个缺陷】。这是「探查数 == 注入数」能对上的前提。
   任一脏行踩到两个检查项，两边的计数就同时错，9/14 验收直接翻车。

3. `charging` 不是随机撒的，是【跨过模拟当前时刻】的会话自然产生的：
   会话 end 超过 sim_now 就截断到 sim_now、kwh 按已充时长折算。
   这样 DWS 的 [start_time, end_time) 永远落在窗口内，不会出现「未来的负荷」。

4. 干净行的 dq_tag 写 'OK' 哨兵，不写空。DWD 必须按规则真清洗、
   最后 drop 掉这一列 —— 不允许拿 dq_tag 当清洗依据，那是自证。

可复现三纪律（改代码务必保持，否则 --seed 是假的）：
  a. 只用传入的 random.Random(seed) 实例，不碰模块级全局随机流
  b. 不迭代 set / dict 做随机选择，要先 sorted() —— Python 的哈希随机化
     会让同一个 seed 在不同进程里产出不同结果
  c. 绝不调 datetime.now()，时间一律来自 Params
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field
from datetime import date, datetime, time, timedelta
from decimal import ROUND_HALF_UP, Decimal
from typing import Iterable

# ---------------------------------------------------------------- 列定义

#: ODS 订单表的列顺序 = charge_order 的 19 列原样 + 末尾 dq_tag。
#: 与 sql/schema.sql 的 charge_order 定义逐字对应。
COLUMNS = [
    "id",
    "order_no",
    "user_id",
    "station_id",
    "pile_id",
    "status",
    "unit_price",
    "start_soc",
    "battery_capacity_kwh",
    "target_soc",
    "reserve_time",
    "start_time",
    "end_time",
    "duration_seconds",
    "kwh",
    "amount",
    "pay_request_id",
    "created_at",
    "updated_at",
    "dq_tag",
]

#: 干净行的哨兵值。不要改成空串 —— 空的在 Spark 里是 null，
#: 会诱导下游用 `dq_tag is not null` 当清洗规则（自证）。
TAG_OK = "OK"

#: 九种脏数据类型。顺序固定，配额按这个顺序分配。
DQ_TAGS = [
    "NULL_START_TIME",   # start_time 为空
    "NULL_KWH",          # kwh 与 amount 为空（表计缺失）
    "NEG_KWH",           # kwh 为负
    "SOC_RANGE",         # target_soc 越界
    "TIME_REVERSED",     # end_time < start_time
    "DUP_ORDER_NO",      # 同一 order_no 两行
    "ORPHAN_STATION",    # station_id 在维表不存在
    "ORPHAN_PILE",       # pile_id 在维表不存在
    "STATUS_CONFLICT",   # settled 但 duration_seconds=0 且 kwh>0
]

#: 假 id 的起点，必须远高于真实 id 上界，避免撞上真站/真桩
FAKE_STATION_BASE = 9001
FAKE_PILE_BASE = 90001

# 单价与功率的物理系数：充电效率
EFFICIENCY = 0.92

# 电池容量档位与权重（度）
BATTERY_CHOICES = [40, 50, 60, 70, 80]
BATTERY_WEIGHTS = [0.18, 0.24, 0.26, 0.20, 0.12]

# 订单状态分布（charging 不在这里 —— 它是跨过 sim_now 自然产生的）
STATUS_WEIGHTS = [("settled", 0.76), ("pending_payment", 0.12),
                  ("cancelled", 0.07), ("reserved", 0.05)]

#: cancelled 里「充上之后才取消」的比例（剩下的都是没开始就取消）
CANCELLED_STARTED_RATIO = 0.40

# 热点站权重（重尾）。桩越少的站越容易被跑满，所以热点优先选小站。
W_VERY_HOT = 3.0
W_HOT = 1.8
W_NORMAL = 1.0
N_VERY_HOT = 4
N_HOT = 4

D2 = Decimal("0.01")
D1 = Decimal("0.1")


def q2(x) -> Decimal:
    """量化到两位小数，四舍五入（Python 内置 round 是银行家舍入，金额不能用）。"""
    return Decimal(str(x)).quantize(D2, rounding=ROUND_HALF_UP)


def q1(x) -> Decimal:
    return Decimal(str(x)).quantize(D1, rounding=ROUND_HALF_UP)


# ---------------------------------------------------------------- 输入维表


@dataclass(frozen=True)
class Station:
    id: int
    code: str          # 形如 SZ003，前两位是城市
    name: str
    price: Decimal     # 该站单价，订单的 unit_price 必须取这里

    @property
    def city(self) -> str:
        return self.code[:2]


@dataclass(frozen=True)
class Pile:
    id: int
    station_id: int
    code: str
    type: str          # fast / slow
    power_kw: Decimal


@dataclass
class Params:
    seed: int
    days: int
    rows: int
    end_date: date
    sim_now: datetime
    dirty_ratio: float = 0.08
    holidays: frozenset = field(default_factory=frozenset)

    def __post_init__(self):
        if self.rows <= 0:
            raise ValueError("rows 必须为正")
        if self.days <= 0:
            raise ValueError("days 必须为正")
        if not (0.0 <= self.dirty_ratio < 0.9):
            raise ValueError("dirty_ratio 应在 [0, 0.9)")
        # sim_now 必须落在 end_date 当天，否则窗口语义就乱了
        if self.sim_now.date() != self.end_date:
            raise ValueError(
                f"sim_now 的日期 {self.sim_now.date()} 与 end_date {self.end_date} 不一致")

    @property
    def start_date(self) -> date:
        return self.end_date - timedelta(days=self.days - 1)


# ---------------------------------------------------------------- 站点权重


def pick_hot_stations(stations: list[Station],
                      pile_count: dict[int, int],
                      n_very: int = N_VERY_HOT,
                      n_hot: int = N_HOT) -> tuple[list[int], list[int]]:
    """挑热点站，跨城市轮转，且优先挑桩数少的（桩少更容易跑满）。

    用户端坐标是运行时由高德解析出来的，热点全挤在一个城市的话，
    演示机解析到别的城市就整片是平的 —— 所以按城市轮转。
    """
    by_city: dict[str, list[Station]] = {}
    for s in stations:
        by_city.setdefault(s.city, []).append(s)

    # 每个城市内部：桩数少的排前面，同桩数按 id —— 全部 sorted，保证确定性
    ordered: list[Station] = []
    for city in sorted(by_city):
        ordered.extend(sorted(by_city[city],
                              key=lambda s: (pile_count.get(s.id, 0), s.id)))

    picked: list[Station] = []
    rank = 0
    while len(picked) < n_very + n_hot:
        # 第 rank 轮：每个城市取还没被选中的第一个
        added = 0
        for city in sorted(by_city):
            city_sorted = sorted(by_city[city],
                                 key=lambda s: (pile_count.get(s.id, 0), s.id))
            if rank < len(city_sorted):
                cand = city_sorted[rank]
                if cand not in picked:
                    picked.append(cand)
                    added += 1
                    if len(picked) >= n_very + n_hot:
                        break
        if added == 0:
            break
        rank += 1

    very = [s.id for s in picked[:n_very]]
    hot = [s.id for s in picked[n_very:n_very + n_hot]]
    return very, hot


def station_weights(stations: list[Station],
                    very_hot: Iterable[int],
                    hot: Iterable[int]) -> dict[int, float]:
    very, h = set(very_hot), set(hot)
    return {
        s.id: (W_VERY_HOT if s.id in very else
               W_HOT if s.id in h else W_NORMAL)
        for s in stations
    }


# ---------------------------------------------------------------- 日内分布


def hour_weights(is_weekend: bool) -> list[float]:
    """24 个小时的到达权重。工作日双峰 8-9 / 17-20，周末压平早高峰。"""
    if not is_weekend:
        base = [0.6, 0.5, 0.4, 0.4, 0.5, 0.8, 1.5, 4.0,
                9.0, 8.0, 4.5, 4.5, 4.0, 4.0, 4.0, 4.5,
                5.5, 8.0, 10.0, 9.0, 7.0, 4.0, 2.0, 1.2]
    else:
        base = [0.8, 0.7, 0.5, 0.4, 0.4, 0.6, 1.0, 1.5,
                2.5, 4.0, 6.0, 6.5, 6.0, 6.0, 6.5, 6.5,
                6.0, 6.0, 6.5, 6.5, 5.5, 4.0, 2.5, 1.5]
    return base


#: 周末总量相对工作日的比例
WEEKEND_FACTOR = 0.85

#: 最后一天给「预约 → 开始」留的余量。有了它，最后一天的 start 也保证
#: 落在 sim_now 之前，于是不需要「抽完再判断、不合格就丢掉」那种写法 ——
#: 丢掉会让总行数补不齐，得靠重抽兜底，可复现性就脆了。
LAST_DAY_MARGIN = timedelta(minutes=45)


def last_day_cutoff(params: Params) -> tuple[int, int]:
    cut = params.sim_now - LAST_DAY_MARGIN
    return cut.hour, cut.minute


# ---------------------------------------------------------------- 会话合成


def _draw_session(rng: random.Random, pile: Pile, price: Decimal
                  ) -> tuple[Decimal, Decimal, Decimal, Decimal, Decimal, float]:
    """给一个桩抽一次会话。

    返回 (kwh, capacity, start_soc, target_soc, amount, duration_h)。

    kwh 是权威值；时长由它反推。不做区间裁剪 —— 反推出来的范围本来就合理
    （fast 约 0.1~0.5h，slow 约 1~12h），裁剪反而会破坏 kwh↔时长自洽。
    """
    capacity = Decimal(rng.choices(BATTERY_CHOICES, weights=BATTERY_WEIGHTS)[0])
    start_soc = q2(min(60.0, max(5.0, rng.gauss(30, 12))))
    # 下限锁 start+25 个百分点：没人会为了 5% 电量特意插枪
    lo = float(start_soc) + 25.0
    target_soc = q2(min(100.0, max(lo, rng.gauss(92, 5))))

    kwh = q2((target_soc - start_soc) / Decimal(100) * capacity)
    if kwh <= 0:                       # 理论上到不了，兜底防脏
        kwh = q2(capacity / 4)
    duration_h = float(kwh) / (float(pile.power_kw) * EFFICIENCY)
    amount = q2(kwh * price)
    return kwh, capacity, start_soc, target_soc, amount, duration_h


def _order_no(d: date, seq: int) -> str:
    """结构化唯一键。随机串迟早碰撞，groupBy 计数就会超过注入数。

    前缀 SIM 也让「业务库没被碰」这个论证一眼可查。
    """
    return f"SIM{d.strftime('%Y%m%d')}{seq:06d}"


# ---------------------------------------------------------------- 主生成


def generate(stations: list[Station],
             piles: list[Pile],
             users: list[int],
             params: Params) -> tuple[list[dict], dict]:
    """生成订单行 + 注入/期望探查计数。

    返回 (rows, report)。rows 是 dict 列表，键为 COLUMNS，
    gen_ods.py 负责按 COLUMNS 顺序落成 CSV。
    """
    rng = random.Random(params.seed)

    if not stations:
        raise ValueError("维表里没有电站")
    if not piles:
        raise ValueError("维表里没有充电桩")

    pile_by_station: dict[int, list[Pile]] = {}
    for p in sorted(piles, key=lambda x: x.id):
        pile_by_station.setdefault(p.station_id, []).append(p)
    # 只保留有桩的站，避免抽到空站再回头重抽（重抽会让可复现性变脆）
    usable = [s for s in sorted(stations, key=lambda x: x.id)
              if pile_by_station.get(s.id)]
    if not usable:
        raise ValueError("没有任何电站拥有充电桩")

    pile_count = {sid: len(ps) for sid, ps in pile_by_station.items()}
    very_hot, hot = pick_hot_stations(usable, pile_count)
    weights = station_weights(usable, very_hot, hot)
    station_by_id = {s.id: s for s in usable}

    # ---- 1. 配额：九类各一份，固定配额而不是按比例随机撒 ----------------
    # 随机会让某些类掉到个位数，且数字随 seed 漂。固定配额才能写进契约。
    total_dirty = int(round(params.rows * params.dirty_ratio))
    quota = max(1, total_dirty // len(DQ_TAGS))
    injected = {tag: quota for tag in DQ_TAGS}

    # 总量口径：rows 含脏行；DUP 那一类算 1 行（多出来的复制行），
    # 其余八类是【原地改】的，不额外占行。
    n_inplace = quota * (len(DQ_TAGS) - 1)      # 8 类原地改
    n_clean = params.rows - quota               # 干净行数（DUP 的复制行另加）
    if n_clean <= n_inplace:
        raise ValueError(f"行数太少：干净行 {n_clean} 装不下 {n_inplace} 个原地注入")

    # ---- 2. 逐日生成干净行 --------------------------------------------
    last_h, last_m = last_day_cutoff(params)
    day_weights = []
    for i in range(params.days):
        d = params.start_date + timedelta(days=i)
        is_weekend = d.weekday() >= 5
        w = WEEKEND_FACTOR if is_weekend else 1.0
        if d in params.holidays:
            w *= 1.15
        if i == params.days - 1:
            # 最后一天只走到 sim_now，有效时段比其他天短，权重也要缩水，
            # 否则这一天的配额补不齐，总行数就少了。
            hw = hour_weights(is_weekend)
            w *= (sum(hw[:last_h]) + hw[last_h] * (last_m / 60.0)) / sum(hw)
        day_weights.append(w)
    sum_w = sum(day_weights)
    per_day = [int(round(n_clean * w / sum_w)) for w in day_weights]
    # 把舍入误差摊到各天，保证总和精确
    drift = n_clean - sum(per_day)
    idx = 0
    while drift != 0:
        step = 1 if drift > 0 else -1
        per_day[idx % params.days] += step
        drift -= step
        idx += 1

    rows: list[dict] = []
    seq = 0
    for i, n_day in enumerate(per_day):
        d = params.start_date + timedelta(days=i)
        is_weekend = d.weekday() >= 5
        hw = hour_weights(is_weekend)
        is_last = (i == params.days - 1)

        for _ in range(n_day):
            st = rng.choices(usable, weights=[weights[s.id] for s in usable])[0]
            pile = rng.choice(pile_by_station[st.id])
            status = rng.choices([s for s, _ in STATUS_WEIGHTS],
                                 weights=[w for _, w in STATUS_WEIGHTS])[0]

            if is_last:
                # 最后一天只抽有效时段，抽出来的一定合格，不用回头丢弃
                hour = rng.choices(range(last_h + 1), weights=hw[:last_h + 1])[0]
                minute = (rng.randrange(last_m + 1) if hour == last_h
                          else rng.randrange(60))
                second = 0
            else:
                hour = rng.choices(range(24), weights=hw)[0]
                minute = rng.randrange(60)
                second = rng.randrange(60)
            reserve = datetime.combine(d, time(hour, minute, second))

            # 预约总在开始之前
            reserve -= timedelta(minutes=rng.randint(3, 90))
            if reserve < datetime.combine(d, time(0, 0)):
                reserve = datetime.combine(d, time(0, 0))

            seq += 1
            row = {c: None for c in COLUMNS}
            row["dq_tag"] = TAG_OK          # 先默认干净，注入阶段再改
            row["order_no"] = _order_no(d, seq)
            row["user_id"] = rng.choice(users)
            row["station_id"] = st.id
            row["pile_id"] = pile.id
            row["unit_price"] = st.price
            row["reserve_time"] = reserve
            row["created_at"] = reserve
            row["updated_at"] = reserve

            if status == "reserved":
                # 只有预约，没开始。kwh/amount/duration 写 0 而不是空 ——
                # 否则 isNull(kwh) 会多吞掉几千行合法数据。
                row.update(status="reserved", duration_seconds=0,
                           kwh=Decimal("0.00"), amount=Decimal("0.00"))
                rows.append(row)
                continue

            # cancelled 有一部分是「充上之后才取消」
            if status == "cancelled" and rng.random() >= CANCELLED_STARTED_RATIO:
                row.update(status="cancelled", duration_seconds=0,
                           kwh=Decimal("0.00"), amount=Decimal("0.00"))
                rows.append(row)
                continue

            (kwh, capacity, start_soc, target_soc, amount,
             duration_h) = _draw_session(rng, pile, st.price)
            start = reserve + timedelta(minutes=rng.randint(2, 25))
            end = start + timedelta(seconds=int(duration_h * 3600))

            if is_last and end > params.sim_now:
                # 会话跨过模拟当前时刻 → 正在充电。截断到 sim_now，
                # kwh 按已充时长折算。这样 [start, end) 永远落在窗口内，
                # DWS 就不会出现「未来的负荷」。
                elapsed = (params.sim_now - start).total_seconds()
                frac = elapsed / max(1.0, (end - start).total_seconds())
                end = params.sim_now
                kwh = q2(kwh * Decimal(str(frac)))
                amount = q2(kwh * st.price)
                status = "charging"
                duration_seconds = int(elapsed)
            else:
                duration_seconds = int((end - start).total_seconds())

            row.update(
                status=status,
                start_soc=start_soc,
                battery_capacity_kwh=capacity,
                target_soc=target_soc,
                start_time=start,
                end_time=end,
                duration_seconds=duration_seconds,
                kwh=kwh,
                amount=amount,
                updated_at=end,
            )
            rows.append(row)

    # ---- 3. 注入八类原地缺陷 ------------------------------------------
    # rng.sample 保证不重复抽取，于是每个脏行只被改一次。
    idx_pool = list(range(len(rows)))
    rng.shuffle(idx_pool)
    cursors = 0
    injected_idx: dict[str, list[int]] = {}
    for tag in DQ_TAGS:
        if tag == "DUP_ORDER_NO":
            continue
        picked = idx_pool[cursors:cursors + quota]
        cursors += quota
        injected_idx[tag] = picked

    def _pick_started(pool: list[int]) -> list[int]:
        """挑出「有开始时间」的行 —— 只有这些行才谈得上时间类缺陷。"""
        return [i for i in pool if rows[i]["start_time"] is not None]

    started_pool = _pick_started(idx_pool[cursors:])
    if len(started_pool) < quota * 6:
        raise ValueError("可用的已开始订单不足，无法注入时间类缺陷")

    # 六类需要 start_time 非空的缺陷，从 started_pool 里依次切
    need_started = ["NULL_KWH", "NEG_KWH", "SOC_RANGE",
                    "TIME_REVERSED", "ORPHAN_STATION", "ORPHAN_PILE",
                    "STATUS_CONFLICT"]
    by_tag: dict[str, list[int]] = {}
    cur = 0
    for tag in need_started:
        by_tag[tag] = started_pool[cur:cur + quota]
        cur += quota

    real_station_ids = {s.id for s in usable}
    real_pile_ids = {p.id for p in piles if p.station_id in real_station_ids}

    for i in by_tag["NULL_KWH"]:
        rows[i]["kwh"] = None
        rows[i]["amount"] = None
        rows[i]["dq_tag"] = "NULL_KWH"
        # start/end/duration 保持正常 —— 否则 isNull(start_time) 会连它一起吞掉

    for i in by_tag["NEG_KWH"]:
        rows[i]["kwh"] = q2(-abs(float(rows[i]["kwh"])))
        rows[i]["amount"] = q2(-abs(float(rows[i]["amount"])))
        rows[i]["dq_tag"] = "NEG_KWH"

    for k, i in enumerate(by_tag["SOC_RANGE"]):
        # 只动 target_soc。两列都越界的话，计数取决于 SQL 写了哪一列。
        rows[i]["target_soc"] = Decimal("-5.00") if k % 2 == 0 else Decimal("105.00")
        rows[i]["dq_tag"] = "SOC_RANGE"

    for i in by_tag["TIME_REVERSED"]:
        # 两端都非空，严格 end < start（契约里写明用严格小于，
        # 若下游写成 <= 会把这行和 STATUS_CONFLICT 一起数两次）
        start = rows[i]["start_time"]
        rows[i]["end_time"] = start - timedelta(seconds=600)
        rows[i]["dq_tag"] = "TIME_REVERSED"

    for k, i in enumerate(by_tag["ORPHAN_STATION"]):
        # 只换 station_id，pile_id 保持真实 → 只有 station 的 anti-join 抓得到
        rows[i]["station_id"] = FAKE_STATION_BASE + k
        rows[i]["dq_tag"] = "ORPHAN_STATION"

    for k, i in enumerate(by_tag["ORPHAN_PILE"]):
        # 只换 pile_id，station_id 保持真实 → 只有 pile 的 anti-join 抓得到
        rows[i]["pile_id"] = FAKE_PILE_BASE + k
        rows[i]["dq_tag"] = "ORPHAN_PILE"

    for i in by_tag["STATUS_CONFLICT"]:
        # end - start 保持正常的 1200s，只把 duration_seconds 归零。
        # 若把 start==end 造成自洽，下游写 <= 时这行会被数两次。
        rows[i]["status"] = "settled"
        rows[i]["duration_seconds"] = 0
        if rows[i]["kwh"] is None or float(rows[i]["kwh"]) <= 0:
            rows[i]["kwh"] = q2(10.0)
            rows[i]["amount"] = q2(10.0 * float(rows[i]["unit_price"]))
        rows[i]["dq_tag"] = "STATUS_CONFLICT"

    # NULL_START_TIME 从「尚未被改过的干净行」里挑 —— 上面那批都动过 start 相关字段
    used = {i for lst in by_tag.values() for i in lst}
    free_clean = [i for i in idx_pool if i not in used and rows[i]["dq_tag"] == TAG_OK]
    if len(free_clean) < quota:
        raise ValueError("干净行不足，无法注入 NULL_START_TIME")
    for i in free_clean[:quota]:
        rows[i]["start_time"] = None
        rows[i]["dq_tag"] = "NULL_START_TIME"
    by_tag["NULL_START_TIME"] = free_clean[:quota]

    # ---- 4. 重复单号：追加复制行 --------------------------------------
    used |= set(by_tag["NULL_START_TIME"])
    dup_sources = [i for i in idx_pool if i not in used
                   and rows[i]["dq_tag"] == TAG_OK
                   and rows[i]["start_time"] is not None]
    if len(dup_sources) < quota:
        raise ValueError("干净行不足，无法注入 DUP_ORDER_NO")
    dup_sources = dup_sources[:quota]

    for i in dup_sources:
        src = rows[i]
        copy = dict(src)
        # created_at 与 updated_at 都要严格更早。矩阵说「保留 updated_at/created_at
        # 最新」——只钉一个的话，下游按另一个排序就会把复制行留下，
        # DWD 里就会残留带 dq_tag 的行。
        copy["created_at"] = src["created_at"] - timedelta(seconds=3600)
        copy["updated_at"] = src["updated_at"] - timedelta(seconds=1800)
        copy["dq_tag"] = "DUP_ORDER_NO"
        rows.append(copy)
    injected_idx["DUP_ORDER_NO"] = dup_sources

    # ---- 5. 统一发 id --------------------------------------------------
    # 放在追加复制行【之后】顺序发号，天然不会和复制行撞 id。
    # 复用原 id 会凭空多出「主键重复」这个第七类缺陷，演示时一定被问到。
    for n, r in enumerate(rows, start=1):
        r["id"] = n

    # ---- 6. pay_request_id：只有已结算的才有 --------------------------
    for n, r in enumerate(rows):
        if r["status"] == "settled" and r["kwh"] is not None and float(r["kwh"]) > 0:
            r["pay_request_id"] = f"PAY{r['order_no']}"
        else:
            r["pay_request_id"] = None

    report = _build_report(rows, injected, quota, params, usable, real_station_ids,
                           real_pile_ids, very_hot, hot, pile_by_station)
    return rows, report


# ---------------------------------------------------------------- 自检


def qa_scan(rows: list[dict],
            station_ids: set[int],
            pile_ids: set[int]) -> dict[str, int]:
    """纯 Python 复刻 NO.113 的那几条探查检查。

    这是「注入数 == 探查数」的核心验证手段：不靠人眼看，跑一遍对数字。
    """
    dup_groups: dict[str, int] = {}
    for r in rows:
        dup_groups[r["order_no"]] = dup_groups.get(r["order_no"], 0) + 1

    return {
        "ods_rows": len(rows),
        "null_start_time": sum(1 for r in rows if r["start_time"] is None),
        "null_kwh": sum(1 for r in rows if r["kwh"] is None),
        "negative_kwh": sum(1 for r in rows
                            if r["kwh"] is not None and float(r["kwh"]) < 0),
        "soc_range": sum(1 for r in rows if r["target_soc"] is not None
                         and not (0 <= float(r["target_soc"]) <= 100)),
        "time_reversed": sum(1 for r in rows
                             if r["start_time"] and r["end_time"]
                             and r["end_time"] < r["start_time"]),
        "dup_order_no": sum(1 for n in dup_groups.values() if n > 1),
        "dup_extra_rows": sum(n - 1 for n in dup_groups.values() if n > 1),
        "orphan_station": sum(1 for r in rows if r["station_id"] not in station_ids),
        "orphan_pile": sum(1 for r in rows if r["pile_id"] not in pile_ids),
        "status_conflict": sum(1 for r in rows
                               if r["status"] == "settled"
                               and r["duration_seconds"] == 0
                               and r["kwh"] is not None
                               and float(r["kwh"]) > 0),
    }


def clean_to_dwd(rows: list[dict],
                 station_ids: set[int],
                 pile_ids: set[int]) -> list[dict]:
    """按 NO.114 的四条规则清洗，用于算出精确的 dwd_rows。

    规则来自需求矩阵：
      丢弃 start_time 为空 / kwh 为空 / kwh<0 / 时间颠倒 / 孤儿；重复单号留最新一条。
    注意 SOC 越界【不丢弃】（矩阵口径是「裁剪或置空」），所以它不减行数。
    """
    kept = []
    for r in rows:
        if r["start_time"] is None:
            continue
        if r["kwh"] is None or float(r["kwh"]) < 0:
            continue
        if r["end_time"] is not None and r["end_time"] < r["start_time"]:
            continue
        if r["station_id"] not in station_ids:
            continue
        if r["pile_id"] not in pile_ids:
            continue
        kept.append(r)

    # 重复单号留 updated_at 最新的一行；并排掉 created_at 作为次序
    best: dict[str, dict] = {}
    for r in kept:
        cur = best.get(r["order_no"])
        key = (r["updated_at"], r["created_at"])
        if cur is None or key > (cur["updated_at"], cur["created_at"]):
            best[r["order_no"]] = r
    return [best[n] for n in sorted(best)]


def _build_report(rows, injected, quota, params, usable, station_ids,
                  pile_ids, very_hot, hot, pile_by_station) -> dict:
    probe = qa_scan(rows, station_ids, pile_ids)
    dwd = clean_to_dwd(rows, station_ids, pile_ids)

    # 业务上天然就没有 start_time 的行（reserved / 未开始的 cancelled）。
    # 这些不是脏数据，但 isNull(start_time) 一样会数到它们 ——
    # 所以契约里必须把「注入数」和「期望探查数」分开写清楚。
    natural_null_start = sum(
        1 for r in rows if r["start_time"] is None and r["dq_tag"] == TAG_OK)

    injected_view = dict(injected)
    injected_view["DUP_ORDER_NO"] = quota
    injected_view["_total_dirty_rows"] = quota * len(DQ_TAGS)

    expected = dict(probe)
    expected["_note"] = ("expected_probe 是 NO.113 探查脚本应当返回的数字；"
                         "其中 null_start_time 含业务天然的未开始订单")

    def _kwh_sum(rs):
        return str(q2(sum(float(r["kwh"] or 0) for r in rs)))

    #: DWD 各 dq_tag 的行数。用来把「哪些脏类进了 DWD」摊开 —— 只违反
    #: 范围/自洽检查的 SOC_RANGE / STATUS_CONFLICT 不被丢弃，会留在这里。
    dwd_by_tag: dict[str, int] = {}
    for r in dwd:
        dwd_by_tag[r["dq_tag"]] = dwd_by_tag.get(r["dq_tag"], 0) + 1

    #: 跨过模拟当前时刻、按已充时长折算过 kwh 的会话。
    #: 判据是 status == charging 且 end_time == sim_now —— 这正是截断分支
    #: 写出来的样子。它们不满足 kwh == (target-start)/100*capacity 这条【整段】
    #: 关系式，下游断言时要排掉。
    #: 另记一个【干净行】子集：脏行里也有几条碰巧落在 sim_now 上（复制行会
    #: 继承时间戳），拿全量 57 去对会多出几行。
    truncated = [r for r in rows
                 if r["status"] == "charging" and r["end_time"] == params.sim_now]
    truncated_clean = sum(1 for r in truncated if r["dq_tag"] == TAG_OK)

    # 矩阵对 STATUS_CONFLICT 的清洗口径【自相矛盾】，两个数都算出来备查：
    #   60-72 行场景表「清洗规则（DWD）」列 → 状态矛盾「丢弃或按规则重算
    #                                          （本阶段丢弃）」= 丢弃
    #   NO.114 详细说明 → 「丢弃空 start/负 kwh/时间颠倒/孤儿；重复 order_no
    #                       留最新」= 没列它 = 保留
    # 本实现跟 NO.114 的行级清单（保留）。不管组长最后定哪个，两个数都在
    # 契约里，9/14 对数字时不会因为记账口径不同而假失败。
    alt_rows = len(dwd) - dwd_by_tag.get("STATUS_CONFLICT", 0)
    alt_kwh = _kwh_sum([r for r in dwd if r["dq_tag"] != "STATUS_CONFLICT"])

    peak = {}
    for sid in very_hot + hot:
        peak[str(sid)] = {
            "station_name": next((s.name for s in usable if s.id == sid), ""),
            "total_piles": len(pile_by_station.get(sid, [])),
        }

    return {
        "params": {
            "seed": params.seed,
            "days": params.days,
            "rows": params.rows,
            "dirty_ratio": params.dirty_ratio,
            "start_date": params.start_date.isoformat(),
            "end_date": params.end_date.isoformat(),
            "sim_now": params.sim_now.strftime("%Y-%m-%d %H:%M:%S"),
            "holidays": sorted(d.isoformat() for d in params.holidays),
        },
        "injected": injected_view,
        "expected_probe": expected,
        "derived": {
            "clean_rows": sum(1 for r in rows if r["dq_tag"] == TAG_OK),
            "dirty_rows": sum(1 for r in rows if r["dq_tag"] != TAG_OK),
            "dwd_rows": len(dwd),
            "dwd_by_tag": dict(sorted(dwd_by_tag.items())),
            "dwd_rows_if_status_conflict_dropped": alt_rows,
            "dwd_kwh_if_status_conflict_dropped": alt_kwh,
            "truncated_charging_rows": len(truncated),
            "truncated_charging_clean": truncated_clean,
            "natural_null_start": natural_null_start,
            "ods_kwh_total": _kwh_sum(rows),
            "dwd_kwh_total": _kwh_sum(dwd),
        },
        "dimension": {
            "stations": len(usable),
            "piles": len(pile_ids),
            "station_id_range": [min(station_ids), max(station_ids)],
            "pile_id_range": [min(pile_ids), max(pile_ids)],
            "station_73_included": 73 in station_ids,
        },
        "hot_stations": {
            "very_hot": very_hot,
            "hot": hot,
            "detail": peak,
        },
        "contract": {
            "timezone": "每个作业都设 spark.sql.session.timeZone=Asia/Shanghai。"
                        "CSV 里是【无时区的裸时间串】，Spark 按会话时区解析、"
                        "也按会话时区渲染 —— 同一个作业里两头一致，所以只读一次、"
                        "同一次算，看不出差别。危险的是【跨作业不一致】：DWS 用一个"
                        "时区写 parquet / 写 JDBC，ADS 用另一个时区读回来，"
                        "时间就整体挪 8 小时（已实测：上海 8 点会渲染成 UTC 0 点），"
                        "早高峰 8-9 的曲线整段错位；拿裸时间串去和 "
                        "current_timestamp() 这类真实时刻比大小同理",
            "read_csv": "header=True + 显式 schema，不要用 inferSchema",
            "columns_by_name": "按列名取列；Spark 列名大小写敏感",
            "anti_joins": "station 和 pile 两个 anti-join 都要写。只写 pile 的话"
                          " ORPHAN_STATION 会漏进 DWD，DWS 冒出 station_id=9001，"
                          "而 load_forecast 对 station 有真 FK，写预测时会报错",
            "time_reversed_op": "用严格 < ；写成 <= 会把 STATUS_CONFLICT 数两次",
            "drop_dq_tag": "DWD 之后必须 drop 掉 dq_tag，且不许拿它当清洗依据",
            "dwd_soc_range_kept": "SOC_RANGE 那些行【不丢弃】—— 矩阵口径是「裁剪或"
                                  "置空」不是丢。丢掉的话 DWD 行数与 kwh 总量都会对"
                                  "不上契约的 dwd_rows / dwd_kwh_total",
            "dwd_composition": "DWD 里留了哪些行看 derived.dwd_by_tag：干净行 + "
                               "SOC_RANGE + STATUS_CONFLICT。后两类只违反「范围/自洽」"
                               "检查，不在四条丢弃规则里，所以【必须】留着",
            "status_conflict_ambiguity": "⚠️ 矩阵两处对 STATUS_CONFLICT 口径不一致："
                                         "60-72 行场景表的「清洗规则（DWD）」列写「本阶段"
                                         "丢弃」，而 NO.114 详细说明的丢弃清单里没列它。"
                                         "本实现按 NO.114 的清单【保留】，dwd_rows / "
                                         "dwd_kwh_total 就是保留后的数；若组长裁定要丢，"
                                         "用 derived.dwd_rows_if_status_conflict_dropped "
                                         "与 dwd_kwh_if_status_conflict_dropped。两边都"
                                         "算好了，9/14 别因为记账口径不同判成不一致",
            "truncated_charging": "derived.truncated_charging_rows 是 status="
                                  "charging 且 end_time == sim_now 的行 —— 跨过模拟"
                                  "当前时刻、按已充时长折算过的会话，不满足 kwh == "
                                  "(target-start)/100*capacity 这条整段关系式，"
                                  "断言时要排掉。其中干净行是 "
                                  "derived.truncated_charging_clean 条，两者别混用",
        },
    }


def verify(rows: list[dict], report: dict) -> list[str]:
    """生成后的自检。返回问题列表，空列表表示全过。"""
    problems: list[str] = []
    injected = report["injected"]
    probe = report["expected_probe"]

    # 1. 九类逐项对齐（注入数 == 由注入决定的探查数）
    checks = {
        "NULL_START_TIME": ("null_start_time", True),
        "NULL_KWH": ("null_kwh", False),
        "NEG_KWH": ("negative_kwh", False),
        "SOC_RANGE": ("soc_range", False),
        "TIME_REVERSED": ("time_reversed", False),
        "ORPHAN_STATION": ("orphan_station", False),
        "ORPHAN_PILE": ("orphan_pile", False),
        "STATUS_CONFLICT": ("status_conflict", False),
    }
    for tag, (key, has_natural) in checks.items():
        want = injected[tag]
        got = probe[key]
        if has_natural:
            want += report["derived"]["natural_null_start"]
        if got != want:
            problems.append(f"{tag}: 探查 {key}={got}，期望 {want}")

    if probe["dup_order_no"] != injected["DUP_ORDER_NO"]:
        problems.append(f"DUP_ORDER_NO: 探查 {probe['dup_order_no']}，"
                        f"期望 {injected['DUP_ORDER_NO']}")
    if probe["dup_extra_rows"] != injected["DUP_ORDER_NO"]:
        problems.append("重复组数 != 多出来的行数（应每对恰好 2 行）")

    # 2. 每个重复组恰好 2 行
    groups: dict[str, int] = {}
    for r in rows:
        groups[r["order_no"]] = groups.get(r["order_no"], 0) + 1
    bad = [n for n, c in groups.items() if c > 2]
    if bad:
        problems.append(f"有 {len(bad)} 个 order_no 出现超过 2 行，例如 {bad[:3]}")

    # 3. 行数
    if len(rows) != report["params"]["rows"]:
        problems.append(f"总行数 {len(rows)} != 期望 {report['params']['rows']}")

    # 4. 干净行不许踩任何检查项
    station_ids = set(range(report["dimension"]["station_id_range"][0],
                            report["dimension"]["station_id_range"][1] + 1))
    pile_ids = set(range(report["dimension"]["pile_id_range"][0],
                         report["dimension"]["pile_id_range"][1] + 1))
    for r in rows:
        if r["dq_tag"] != TAG_OK:
            continue
        if r["station_id"] not in station_ids:
            problems.append(f"干净行 {r['order_no']} 的 station_id 不在维表内")
            break
    for r in rows:
        if r["dq_tag"] != TAG_OK:
            continue
        if r["pile_id"] not in pile_ids:
            problems.append(f"干净行 {r['order_no']} 的 pile_id 不在维表内")
            break

    # 5. 物理自洽 + 时间不越界（只查已完成的结算单）
    sim_now = datetime.strptime(report["params"]["sim_now"], "%Y-%m-%d %H:%M:%S")
    for r in rows:
        if r["end_time"] is not None and r["end_time"] > sim_now:
            problems.append(f"{r['order_no']} 的 end_time 越过了 sim_now")
            break
    for r in rows:
        if r["dq_tag"] != TAG_OK or r["status"] != "settled":
            continue
        if r["kwh"] is None or r["start_soc"] is None or r["target_soc"] is None:
            continue
        want = float((r["target_soc"] - r["start_soc"]) / Decimal(100)
                     * r["battery_capacity_kwh"])
        if abs(float(r["kwh"]) - want) > 0.05:
            problems.append(f"{r['order_no']} 的 kwh 与 SOC×容量 不一致")
            break
        if float(r["kwh"]) > float(r["battery_capacity_kwh"]) + 1e-6:
            problems.append(f"{r['order_no']} 的 kwh 超过了电池容量")
            break

    return problems
