#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
v5 结构升级三处 DDL 逐字一致性检查（docs/数据.md 一）

三处必须一致：
  1. sql/schema.sql                    —— 空库全量建表走这里
  2. sql/patch_v5_user_groups.sql      —— 已有库升级走这里（幂等 ADD COLUMN）
  3. server/database.cpp upgradeSchema() —— 服务端启动时自动升级走这里

为什么必须是脚本而不是靠人眼看：三份是**独立维护**的文本，改一处忘另外两处
不会报错，只会让「新装的库」和「升级上来的库」结构不一样 —— 大屏在两种库上
跑出两种结果，而且没人知道差在哪。

用法：python3 tests/test_schema_v5_consistency.py     （不连库，纯文本比对）
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCHEMA = os.path.join(ROOT, "sql", "schema.sql")
PATCH = os.path.join(ROOT, "sql", "patch_v5_user_groups.sql")
CPP = os.path.join(ROOT, "server", "database.cpp")

COLUMNS = ["gender", "age_group", "city", "vehicle_type",
           "registration_source", "last_active_at"]
BEFORE_FIRST = "avatar"          # schema.sql 里 gender 前面那一列
NEW_TABLES = ["user_group_profile", "user_group_insight"]
VERSION_DESC = "user 表加用户画像属性 + user_group_profile/user_group_insight 两表"

passed = failed = 0
problems = []


def check(name, ok, detail=""):
    global passed, failed
    if ok:
        passed += 1
        print("  ✅ %s" % name)
    else:
        failed += 1
        problems.append("%s  %s" % (name, detail))
        print("  ❌ %s  %s" % (name, detail))


def norm(s):
    """抹平排版差异：连续空白压成一个空格。"""
    return re.sub(r"\s+", " ", s).strip()


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def cpp_literals(text):
    """把 C++ 相邻字符串字面量拼成一条字符串（"a" "b" -> "ab"）。

    只处理 QStringLiteral( "..." "..." ) 这种形态：逐行找 "..." 段并拼起来。
    """
    out, buf = [], []
    for line in text.splitlines():
        found = re.findall(r'"((?:[^"\\]|\\.)*)"', line)
        if found and ('"' in line):
            buf.extend(found)
            if line.rstrip().endswith('"') or line.rstrip().endswith('"))'):
                if not line.rstrip().endswith(',') and not line.rstrip().endswith('}'):
                    out.append("".join(buf))
                    buf = []
    if buf:
        out.append("".join(buf))
    return out


# ---------------------------------------------------------------------------
# 1. 六列
# ---------------------------------------------------------------------------
def cols_from_schema():
    """-> [(col, 'typedef')]，按 schema.sql 里出现的先后顺序。"""
    text = read(SCHEMA)
    m = re.search(r"CREATE TABLE `user` \((.*?)\n\) ENGINE", text, re.S)
    if not m:
        raise RuntimeError("schema.sql 里找不到 CREATE TABLE `user`")
    out = []
    for line in m.group(1).splitlines():
        line = line.strip().rstrip(",")
        if not line or line.startswith("--"):
            continue
        name = line.split()[0].strip("`")
        out.append((name, norm(line)))
    return out


def cols_from_patch():
    """-> [(col, 'typedef', after, guard)]。

    patch 里每段长这样（ALTER 串在前，COLUMN_NAME 守卫在后）：
        SET @ddl = (SELECT IF(COUNT(*) = 0,
            'ALTER TABLE `user` ADD COLUMN gender ... AFTER avatar',
            'DO 0')
          FROM information_schema.COLUMNS
          WHERE ... AND COLUMN_NAME = 'gender');
    所以按 ALTER 串找，再回头看紧跟的守卫写的是哪个列
    —— 守卫写错列名是最阴的复制粘贴 bug：ALTER 改 A 列，却拿 B 列的存在性做判断。
    """
    text = read(PATCH)
    out = []
    for m in re.finditer(
            r"'ALTER TABLE `user` ADD COLUMN (.*?) AFTER (\w+)',(.*?)COLUMN_NAME = '(\w+)'\)",
            text, re.S):
        body = m.group(1).replace("''", "'")     # SQL 串里的 '' 是转义的 '
        col = body.split()[0]
        out.append((col, norm(body), m.group(2), m.group(4)))
    if not out:
        raise RuntimeError("patch 里没解析出任何 ALTER 串")
    return out


def cols_from_cpp():
    """-> [(col, 'typedef', after)]，从 kUserGroupColumns 数组里取。"""
    text = read(CPP)
    m = re.search(r"kUserGroupColumns\[\] = \{(.*?)\n    \};", text, re.S)
    if not m:
        raise RuntimeError("database.cpp 里找不到 kUserGroupColumns[]")
    body = m.group(1)
    # 每个元素：{"name", "..." "..."},
    out = []
    for elem in re.finditer(r'\{\s*"(\w+)",(.*?)\}\s*,', body, re.S):
        col, rest = elem.group(1), elem.group(2)
        ddl = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', rest))
        mm = re.match(r"ALTER TABLE `user` ADD COLUMN (.*?) AFTER (\w+)", ddl)
        if not mm:
            raise RuntimeError("%s 的 DDL 解析失败：%s" % (col, ddl))
        out.append((col, norm(mm.group(1)), mm.group(2)))
    return out


def test_columns():
    print("\n[1] user 表六列：三处 DDL 逐字一致")
    s, p, c = cols_from_schema(), cols_from_patch(), cols_from_cpp()

    for label, rows in (("schema.sql", s), ("patch", p), ("database.cpp", c)):
        got = [r[0] for r in rows]
        missing = [x for x in COLUMNS if x not in got]
        check("%s 六列齐全" % label, not missing,
              "缺 %s" % missing if missing else "6/6")

    # patch 的列名来自 ALTER 串本身，守卫里的列名必须一致
    bad_guard = [col for col, _d, _a, g in p if g != col]
    check("patch 的 COLUMN_NAME 守卫与 ALTER 的列名一致", not bad_guard,
          "不一致：%s" % bad_guard if bad_guard else "%d 段" % len(p))

    order = [n for n, _d in s]
    sd = dict((n, d) for n, d in s)
    for label, rows in (("patch", p), ("database.cpp", c)):
        bad = [(n, sd.get(n), d) for n, d, _a, *_r in rows if sd.get(n) != d]
        check("%s 的列定义与 schema.sql 逐字一致" % label, not bad,
              "; ".join("%s: %r != %r" % b for b in bad) if bad else "")

    # ALTER 的 AFTER 必须等于 schema.sql 里**紧挨着排在它前面**的那一列。
    # 注意这六列在 schema.sql 里并不连续：balance/status/created_at/
    # updated_at/last_login_at 夹在中间，last_active_at 的前一列是
    # last_login_at 而不是 registration_source。所以不能拿「上一个 v5 列」
    # 当期望值 —— 位置错了，升级上来的库列序和新装的库不一样，SELECT * 都不同。
    for label, rows in (("patch", p), ("database.cpp", c)):
        bad = []
        for row in rows:
            col, after = row[0], row[2]
            i = order.index(col)
            expect = order[i - 1] if i > 0 else None
            if after != expect:
                bad.append("%s AFTER %s（schema 里前一个是 %s）" % (col, after, expect))
        check("%s 的 AFTER 与 schema 列序一致" % label, not bad,
              "; ".join(bad) if bad else "")
        pos = [order.index(x) for x in [r[0] for r in rows] if x in order]
        check("%s 的列在 schema.sql 里顺序相同" % label,
              pos == sorted(pos) and len(pos) == len(rows),
              "schema 中位置 %s" % pos)


# ---------------------------------------------------------------------------
# 2. 两张新表
# ---------------------------------------------------------------------------
def ddl_from_schema(name):
    """schema.sql 里那张表的 CREATE 语句（真实换行）。"""
    m = re.search(
        r"(CREATE TABLE (?:IF NOT EXISTS )?%s \(.*?\n\) ENGINE[^\n;]*)" % name,
        read(SCHEMA), re.S)
    if not m:
        raise RuntimeError("schema.sql 里找不到 %s 的 CREATE TABLE" % name)
    return m.group(1)


def ddl_from_cpp(name):
    """database.cpp 里那张表的 CREATE 语句。

    C++ 侧是 QStringLiteral( "..." "..." ) 相邻字面量拼接，换行写成 \\n 转义，
    所以先把这几行的字面量按顺序拼起来，再把 \\n 还原成真换行，
    之后就能和 schema.sql 用同一个解析器。
    """
    text = read(CPP)
    m = re.search(
        r'"CREATE TABLE IF NOT EXISTS %s \(\\n"(?:.*?)\)\)\)' % name, text, re.S)
    if not m:
        raise RuntimeError("database.cpp 里找不到 %s 的 CREATE TABLE" % name)
    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(0))
    return "".join(literals).replace("\\n", "\n")


def table_body(ddl, name):
    """从一条 CREATE TABLE 语句里取出表体，返回归一化后的行列表。"""
    m = re.search(r"CREATE TABLE (?:IF NOT EXISTS )?%s \((.*?)\n\) ENGINE" % name,
                  ddl, re.S)
    if not m:
        raise RuntimeError("表体解析失败：%s" % name)
    return [norm(l) for l in m.group(1).splitlines() if l.strip()]


def test_tables():
    print("\n[2] 两张新表：schema.sql 与 database.cpp 表体一致")
    for name in NEW_TABLES:
        a = table_body(ddl_from_schema(name), name)
        b = table_body(ddl_from_cpp(name), name)
        if a == b:
            check("%s 表体 %d 行一致" % (name, len(a)), True)
            continue
        diff = []
        for i in range(max(len(a), len(b))):
            x = a[i] if i < len(a) else "<缺>"
            y = b[i] if i < len(b) else "<缺>"
            if x != y:
                diff.append("第%d行 %r != %r" % (i + 1, x, y))
        check("%s 表体一致" % name, False, "; ".join(diff[:4]))


# ---------------------------------------------------------------------------
# 3. schema_version 第 5 条的 description
# ---------------------------------------------------------------------------
def test_version():
    print("\n[3] schema_version 第 5 条：三处文本一致")
    s = read(SCHEMA)
    p = read(PATCH)
    c = read(CPP)

    def desc_of(text, name):
        m = re.search(r"\(5,\s*'([^']*)'", text)
        if not m:
            raise RuntimeError("%s 里找不到第 5 条版本" % name)
        return m.group(1)

    ds, dp, dc = desc_of(s, "schema.sql"), desc_of(p, "patch"), desc_of(c, "database.cpp")
    check("schema.sql 与 patch 一致", ds == dp, "%r != %r" % (ds, dp))
    check("schema.sql 与 database.cpp 一致", ds == dc, "%r != %r" % (ds, dc))
    check("文本就是约定的那句", ds == VERSION_DESC, "%r" % ds)

    # 版本号必须是 5，且三个文件都用它
    for name, text in (("schema.sql", s), ("patch", p), ("database.cpp", c)):
        check("%s 里版本号为 5" % name, bool(re.search(r"\(5,\s*'", text)))


def main():
    print("=" * 72)
    print("v5 三处 DDL 一致性检查")
    print("=" * 72)
    test_columns()
    test_tables()
    test_version()

    print("\n" + "=" * 72)
    if failed:
        print("❌ %d 项通过，%d 项失败：" % (passed, failed))
        for p in problems:
            print("   - %s" % p)
        return 1
    print("✅ 全部 %d 项通过" % passed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
