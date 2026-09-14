#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
「用户群体管理大屏」演示数据自检（docs/数据.md 一、二）

只连数据库，**不需要起 charging-server**。

用法：
  python3 tests/test_seed_user_groups_demo.py          # 含幂等复跑（默认）
  python3 tests/test_seed_user_groups_demo.py --dry    # 只读校验，不改动库

幂等那一步会把 sql/seed_user_groups_demo.sql 再执行一遍。这是安全的：
该脚本按手机号段 / 单号前缀做 WHERE NOT EXISTS，数据已在时一行都不写，
跑完前后行数应完全相同。若你不想让它碰库，用 --dry。

⚠️ 口径必须和生成器 sql/gen_user_groups_demo.py 的 3.1/3.3/3.4 段**逐字一致**
（尤其是「近30天」只算已结算 + end_time 边界），否则占比会两边对不上。
"""

import os
import subprocess
import sys

DB = ["mysql", "-u", "charging_user", "-p123456",
      "--default-character-set=utf8mb4", "charging_system", "-N", "-B"]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEED = os.path.join(ROOT, "sql", "seed_user_groups_demo.sql")

N_ALL = 120          # 全库用户目标
N_NEW = 80           # 本批新增
N_ORDERS = 447       # 本批订单
PHONE_BATCH = "1370000%"           # 本批手机号段
ORDER_BATCH = "UG"                 # 本批单号前缀
DEMO_PHONE = "13800138001"

# docs/数据.md 二.3 的建议区间（全库口径，人数 -> 百分比区间）
FREQ_RANGE = {"high": (10, 15), "medium": (25, 30), "low": (30, 35), "inactive": (20, 30)}
PREF_RANGE = {"fast": (35, 45), "slow": (20, 30), "mixed": (25, 35)}
VALUE_N = {"high": 24, "medium": 60, "low": 36}

passed = failed = 0
problems = []


def db(sql, params=None):
    """跑一条 SQL，返回 list[list[str]]（TSV）。"""
    if params:
        sql = sql % params
    r = subprocess.run(DB + ["-e", sql], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("SQL 失败：%s\n%s" % (sql.strip()[:200], r.stderr.strip()))
    out = r.stdout.rstrip("\n")
    return [line.split("\t") for line in out.split("\n")] if out else []


def one(sql, params=None):
    rows = db(sql, params)
    return rows[0][0] if rows else None


def check(name, ok, detail=""):
    global passed, failed
    if ok:
        passed += 1
        print("  ✅ %s%s" % (name, ("  " + detail) if detail else ""))
    else:
        failed += 1
        problems.append("%s  %s" % (name, detail))
        print("  ❌ %s  %s" % (name, detail))


def run_seed():
    """重跑 seed（幂等性验证用）。"""
    with open(SEED, "rb") as f:
        r = subprocess.run(DB[:-4] + ["charging_system"], stdin=f,
                           capture_output=True)
    if r.returncode:
        raise RuntimeError("seed 执行失败：\n" + r.stderr.decode("utf-8", "replace"))


# ---------------------------------------------------------------------------
# 一、v5 结构升级到位
# ---------------------------------------------------------------------------
def test_schema_v5():
    print("\n[1] v5 结构升级（docs/数据.md 一）")
    ver = int(one("SELECT MAX(version) FROM schema_version"))
    check("schema_version 最高为 5", ver >= 5, "实测 %d" % ver)

    want_cols = {"gender", "age_group", "city", "vehicle_type",
                 "registration_source", "last_active_at"}
    got = {r[0] for r in db(
        "SELECT COLUMN_NAME FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'user'")}
    check("user 表六列齐全", want_cols <= got,
          "缺 %s" % sorted(want_cols - got) if want_cols - got else "6/6")

    tables = {r[0] for r in db(
        "SELECT TABLE_NAME FROM information_schema.TABLES "
        "WHERE TABLE_SCHEMA = DATABASE()")}
    missing = {"user_group_profile", "user_group_insight"} - tables
    check("两张画像表存在", not missing, "缺 %s" % sorted(missing) if missing else "2/2")


# ---------------------------------------------------------------------------
# 二、幂等：重跑 seed 前后行数零变化
# ---------------------------------------------------------------------------
SNAPSHOT = """
SELECT (SELECT COUNT(*) FROM `user`)                                       ,
       (SELECT COUNT(*) FROM `user` WHERE phone LIKE '{p}')                ,
       (SELECT COUNT(*) FROM charge_order)                                 ,
       (SELECT COUNT(*) FROM charge_order WHERE order_no LIKE BINARY '{o}%'),
       (SELECT COUNT(*) FROM charge_order
         WHERE order_no LIKE BINARY '{o}%' AND status = 'settled')
""".format(p=PHONE_BATCH, o=ORDER_BATCH)

SNAP_NAMES = ["全库用户", "本批用户", "全库订单", "本批订单", "本批已结算"]


def test_idempotent(dry):
    print("\n[2] 幂等：重复执行 seed 行数不变")
    if dry:
        print("  ⏭  --dry，跳过复跑（未验证幂等）")
        return
    before = db(SNAPSHOT)[0]
    run_seed()
    after = db(SNAPSHOT)[0]
    for name, b, a in zip(SNAP_NAMES, before, after):
        check("复跑后 %s 不变" % name, b == a, "%s -> %s" % (b, a))


# ---------------------------------------------------------------------------
# 三、三组比例（全库 120 人口径）
# ---------------------------------------------------------------------------
# 与生成器 3.1 逐字同款：近30天 = 已结算 且 end_time >= CURDATE() - 30 DAY
N30_SQL = """
SELECT u.id, COALESCE(d.c, 0), COALESCE(d.s, 0)
FROM `user` u
LEFT JOIN (SELECT user_id, COUNT(*) c, SUM(amount) s FROM charge_order
           WHERE status = 'settled' AND end_time >= CURDATE() - INTERVAL 30 DAY
           GROUP BY user_id) d ON d.user_id = u.id
"""

# 与生成器 3.3 逐字同款：按已结算单的桩类型占比定 fast/slow/mixed，无单为 mixed
PREF_SQL = """
SELECT u.id, COALESCE(t.nf, 0), COALESCE(t.ns, 0) FROM `user` u
LEFT JOIN (SELECT o.user_id, SUM(p.type = 'fast') nf, SUM(p.type = 'slow') ns
           FROM charge_order o JOIN pile p ON p.id = o.pile_id
           WHERE o.status = 'settled' GROUP BY o.user_id) t ON t.user_id = u.id
"""


def load_n30():
    return {int(r[0]): (int(r[1]), float(r[2])) for r in db(N30_SQL)}


def load_pref():
    """-> {user_id: (pref, 已结算单总数)}。总数是判定 mixed 可行性的依据。"""
    out = {}
    for r in db(PREF_SQL):
        nf, ns = int(r[1]), int(r[2])
        n = nf + ns
        if n == 0:
            pref = "mixed"
        elif nf / n >= 0.6:
            pref = "fast"
        elif nf / n <= 0.4:
            pref = "slow"
        else:
            pref = "mixed"
        out[int(r[0])] = (pref, n)
    return out


def pct(n, total):
    return n * 100.0 / total


def test_frequency():
    print("\n[3] 频率分层（近30天已结算单数，全库 120 人）")
    n30 = load_n30()
    check("覆盖全库用户", len(n30) == N_ALL, "实测 %d 人" % len(n30))

    freq = {"high": 0, "medium": 0, "low": 0, "inactive": 0}
    for _uid, (c, _amt) in n30.items():
        if c >= 8:
            freq["high"] += 1
        elif c >= 3:
            freq["medium"] += 1
        elif c >= 1:
            freq["low"] += 1
        else:
            freq["inactive"] += 1

    for k, (lo, hi) in FREQ_RANGE.items():
        p = pct(freq[k], N_ALL)
        check("频率 %-8s %3d 人 = %5.1f%%" % (k, freq[k], p),
              lo <= p <= hi, "要求 %d~d%%" % (lo, hi) if not (lo <= p <= hi) else "")


def test_pref():
    print("\n[4] 快慢充分层（已结算单的桩类型占比）")
    pref = load_pref()
    check("覆盖全库用户", len(pref) == N_ALL, "实测 %d 人" % len(pref))

    cnt = {"fast": 0, "slow": 0, "mixed": 0}
    for v, _n in pref.values():
        cnt[v] += 1
    for k, (lo, hi) in PREF_RANGE.items():
        p = pct(cnt[k], N_ALL)
        check("快慢充 %-6s %3d 人 = %5.1f%%" % (k, cnt[k], p),
              lo <= p <= hi, "要求 %d~%d%%" % (lo, hi) if not (lo <= p <= hi) else "")

    # mixed 只有 1 笔已结算单时不可能出现（1 单占比必是 0%/100%）。
    # 2 笔可以：1 快 1 慢 = 50%，落在 40%~60% 中间；2:1=66.7% 已被判成 fast。
    # 可行性看的是**已结算单总数**，不是近30天数 —— 两处口径不同，别混。
    bad = [uid for uid, (v, n) in pref.items() if v == "mixed" and n == 1]
    check("mixed 都不是只有 1 单的用户", not bad,
          "命中 %d 人" % len(bad) if bad else "")
    check("本批用户都有已结算单（无 n=0 兜底成 mixed）",
          not [uid for uid, (_v, n) in pref.items() if n == 0])


def test_value():
    print("\n[5] 价值分层（全库 120 人按近30天金额排名，0 消费并列垫底）")
    n30 = load_n30()
    # 与生成器 3.4 同款：ROW_NUMBER over 全体用户，0 消费并列垫底靠 user_id 破
    ranked = sorted(n30.items(), key=lambda kv: (-kv[1][1], kv[0]))
    check("参与排名人数", len(ranked) == N_ALL, "实测 %d" % len(ranked))

    # 零消费必须整段并排在尾部（名次靠后的一定不比前面金额高）
    tail_zero = all(ranked[i][1][1] <= ranked[i - 1][1][1] for i in range(1, N_ALL))
    check("金额名次单调不增（0 消费并列在尾部）", tail_zero)

    # 名次切点即分层；人数是切法的定义，真正要验的是「切点金额落对了带」：
    # 第 24 名必须够得上 band H 下界 80，第 84 名必须够得上 band M 下界 8，
    # 否则名次对了、金额带错了，大屏上仍是「高价值用户只花了 3 块钱」。
    check("第 24 名金额 ≥ 80（band H 下界）", ranked[23][1][1] >= 80,
          "实测 %.2f" % ranked[23][1][1])
    check("第 84 名金额 ≥ 8（band M 下界）", ranked[83][1][1] >= 8,
          "实测 %.2f" % ranked[83][1][1])
    n_low = sum(1 for _u, (c, _a) in ranked if c == 0)
    check("零消费用户数 = 低价值人数 36 - 2", n_low == 28,
          "实测 %d 人（低价值里另 2 人是正消费中最小的两个）" % n_low)

    # 阈值法和名次法必须给出同一个分层 —— 两边不一致的话，大屏按金额阈值过滤
    # 和按名次过滤会显示两套数字，评审时说不清哪个对。
    n_ge80 = sum(1 for _u, (_c, a) in ranked if a >= 80)
    n_ge8 = sum(1 for _u, (_c, a) in ranked if a >= 8)
    check("金额 ≥80 的人数 = 24（与名次法一致）", n_ge80 == VALUE_N["high"],
          "实测 %d" % n_ge80)
    check("金额 ≥8 的人数 = 84（与名次法一致）", n_ge8 == VALUE_N["high"] + VALUE_N["medium"],
          "实测 %d" % n_ge8)

    a24, a25 = ranked[23][1][1], ranked[24][1][1]
    a84, a85 = ranked[83][1][1], ranked[84][1][1]
    check("第24/25名金额不并列", a24 > a25, "%.2f > %.2f" % (a24, a25))
    check("第84/85名金额不并列", a84 > a85, "%.2f > %.2f" % (a84, a85))


# ---------------------------------------------------------------------------
# 四、docs/数据.md 二.4 的硬约束
# ---------------------------------------------------------------------------
def test_constraints():
    print("\n[6] 二.4 约束")

    # 演示账号：seed 一行都不该碰它
    r = db("SELECT id, balance, status FROM `user` WHERE phone = '%s'" % DEMO_PHONE)
    check("演示账号存在", bool(r))
    if r:
        uid = r[0][0]
        n_batch = int(one("SELECT COUNT(*) FROM charge_order "
                          "WHERE user_id = %s AND order_no LIKE BINARY '%s%%'"
                          % (uid, ORDER_BATCH)))
        check("演示账号名下无本批订单", n_batch == 0, "实测 %d 笔" % n_batch)
        print("     （演示账号现状：余额 %s / 状态 %s / 订单 %s 笔 —— 仅打印，不做断言，"
              "有人演示充过值属正常）"
              % (r[0][1], r[0][2],
                 one("SELECT COUNT(*) FROM charge_order WHERE user_id = %s" % uid)))

    # 单号与手机号唯一（全库，不只是本批）
    check("全库无重复单号",
          int(one("SELECT COUNT(*) - COUNT(DISTINCT order_no) FROM charge_order")) == 0)
    check("全库无重复手机号",
          int(one("SELECT COUNT(*) - COUNT(DISTINCT phone) FROM `user`")) == 0)

    # 没有把 bigdata/ 那批 ODS 订单导进来。ODS 的前缀是 SIM
    # （bigdata/simulate.py 的 _order_no，注释里写明「前缀 SIM 让业务库没被碰
    # 这个论证一眼可查」），所以直接查 SIM，不要去猜前缀白名单 ——
    # 库里还有 18 笔 32 位随机 hex 单号，那是 charging-server 演示时自己下的真单。
    n_sim = int(one("SELECT COUNT(*) FROM charge_order "
                    "WHERE order_no LIKE BINARY 'SIM%%'"))
    check("订单里没有 SIM 前缀的 ODS 脏数据", n_sim == 0, "命中 %d 行" % n_sim)
    # 本批单号格式：UG + 11 位序号，不带日期 —— 带日期的话隔天重跑会生成
    # 不同单号，WHERE NOT EXISTS 就认不出来，幂等直接失效。
    bad_fmt = int(one("SELECT COUNT(*) FROM charge_order "
                      "WHERE order_no LIKE BINARY '%s%%' "
                      "  AND order_no NOT REGEXP '^%s[0-9]{11}$'" % (ORDER_BATCH, ORDER_BATCH)))
    check("本批单号格式为 %s+11位序号（不含日期）" % ORDER_BATCH,
          bad_fmt == 0, "命中 %d 行" % bad_fmt)

    ug = "order_no LIKE BINARY '%s%%'" % ORDER_BATCH
    dirty = [
        ("外键失效(用户)", "SELECT COUNT(*) FROM charge_order o "
                        "LEFT JOIN `user` u ON u.id = o.user_id "
                        "WHERE %s AND u.id IS NULL" % ug),
        ("外键失效(电桩)", "SELECT COUNT(*) FROM charge_order o "
                        "LEFT JOIN pile p ON p.id = o.pile_id "
                        "WHERE %s AND p.id IS NULL" % ug),
        ("外键失效(站点)", "SELECT COUNT(*) FROM charge_order o "
                        "LEFT JOIN station s ON s.id = o.station_id "
                        "WHERE %s AND s.id IS NULL" % ug),
        ("站点与电桩不匹配", "SELECT COUNT(*) FROM charge_order o JOIN pile p ON p.id = o.pile_id "
                        "WHERE %s AND o.station_id <> p.station_id" % ug),
        ("负电量", "SELECT COUNT(*) FROM charge_order WHERE %s AND kwh < 0" % ug),
        ("负金额", "SELECT COUNT(*) FROM charge_order WHERE %s AND amount < 0" % ug),
        ("金额≠电量×单价", "SELECT COUNT(*) FROM charge_order WHERE %s "
                      "AND ABS(amount - ROUND(kwh * unit_price, 2)) > 0.01" % ug),
        ("时间倒置(end<start)", "SELECT COUNT(*) FROM charge_order WHERE %s "
                            "AND end_time IS NOT NULL AND start_time IS NOT NULL "
                            "AND end_time < start_time" % ug),
        ("未来时间", "SELECT COUNT(*) FROM charge_order WHERE %s "
                  "AND (reserve_time > NOW() OR start_time > NOW() OR end_time > NOW())" % ug),
        ("已结算缺支付号", "SELECT COUNT(*) FROM charge_order WHERE %s AND status = 'settled' "
                      "AND (pay_request_id IS NULL OR pay_request_id = '')" % ug),
        ("已结算缺起止时间", "SELECT COUNT(*) FROM charge_order WHERE %s AND status = 'settled' "
                       "AND (start_time IS NULL OR end_time IS NULL)" % ug),
        ("取消/预约单有电量", "SELECT COUNT(*) FROM charge_order WHERE %s "
                        "AND status IN ('cancelled','reserved') AND kwh > 0" % ug),
        ("订单早于用户注册", "SELECT COUNT(*) FROM charge_order o JOIN `user` u ON u.id = o.user_id "
                       "WHERE %s AND o.reserve_time < u.created_at" % ug),
        ("进行中的单超过 12h", "SELECT COUNT(*) FROM charge_order WHERE %s "
                          "AND status IN ('charging','reserved') "
                          "AND reserve_time < NOW() - INTERVAL 12 HOUR" % ug),
    ]
    for name, sql in dirty:
        n = int(one(sql))
        check(name, n == 0, "命中 %d 行" % n if n else "")

    # 本批用户六列属性都写上了（'unknown' 是允许的合法取值，空串不是）
    blank = int(one(
        "SELECT COUNT(*) FROM `user` WHERE phone LIKE '%s' AND ("
        " gender = '' OR age_group = '' OR city = '' OR vehicle_type = ''"
        " OR registration_source = '' OR last_active_at IS NULL)" % PHONE_BATCH))
    check("本批用户六列属性无空值", blank == 0, "命中 %d 行" % blank)

    # last_active_at 要和频率层级自洽：沉默的确实很久没来，高频的确实近期来过
    n30 = load_n30()
    inactive_ids = [str(u) for u, (c, _a) in n30.items() if c == 0]
    high_ids = [str(u) for u, (c, _a) in n30.items() if c >= 8]
    if inactive_ids:
        stale = int(one("SELECT COUNT(*) FROM `user` WHERE id IN (%s) "
                        "AND last_active_at >= CURDATE() - INTERVAL 30 DAY"
                        % ",".join(inactive_ids)))
        check("inactive 用户最近活跃都在 30 天前", stale == 0, "命中 %d 人" % stale)
    if high_ids:
        fresh = int(one("SELECT COUNT(*) FROM `user` WHERE id IN (%s) "
                        "AND last_active_at < CURDATE() - INTERVAL 30 DAY"
                        % ",".join(high_ids)))
        check("high 用户最近活跃都在 30 天内", fresh == 0, "命中 %d 人" % fresh)


def main():
    dry = "--dry" in sys.argv
    if not os.path.exists(SEED):
        print("找不到 %s，先跑 python3 sql/gen_user_groups_demo.py" % SEED)
        return 1
    n_all = int(one("SELECT COUNT(*) FROM `user`"))
    if n_all < N_ALL:
        print("库里只有 %d 个用户，演示数据还没导入 —— 先执行：\n"
              "  mysql -u charging_user -p123456 charging_system < sql/seed_user_groups_demo.sql"
              % n_all)
        return 1

    print("=" * 74)
    print("「用户群体管理大屏」演示数据自检%s" % ("（--dry 只读模式）" if dry else ""))
    print("=" * 74)

    test_schema_v5()
    test_idempotent(dry)
    test_frequency()
    test_pref()
    test_value()
    test_constraints()

    print("\n" + "=" * 74)
    if failed:
        print("❌ %d 项通过，%d 项失败：" % (passed, failed))
        for p in problems:
            print("   - %s" % p)
        return 1
    print("✅ 全部 %d 项通过" % passed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
