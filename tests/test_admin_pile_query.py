#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NO.38/NO.100 admin_pile_list TCP 端到端筛选/分页/站不存在测试（只读数据库）。"""
import json, socket, struct, subprocess, sys

HOST, PORT = "127.0.0.1", 9000
DB = ["mysql", "-u", "charging_user", "-p123456", "charging_system", "-N", "-B", "-e"]

def db(sql):
    r = subprocess.run(DB + [sql], capture_output=True, text=True)
    if r.returncode: raise RuntimeError(r.stderr.strip())
    return r.stdout.strip()

def recv_n(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n-len(b))
        if not c: raise ConnectionError("对端关闭连接")
        b += c
    return b

def request(obj):
    payload = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(struct.pack(">I", len(payload)) + payload)
        size = struct.unpack(">I", recv_n(s, 4))[0]
        out = json.loads(recv_n(s, size).decode("utf-8"))
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return out

def stats_shape(r):
    s = (r.get("data") or {}).get("stats")
    assert isinstance(s, dict) and "total" in s
    for k in ("idle", "busy", "fault"):
        assert isinstance(s.get(k), dict) and "count" in s[k] and "rate" in s[k]
    assert s["total"] == sum(s[k]["count"] for k in ("idle", "busy", "fault"))
    return {k: s[k] for k in ("idle", "busy", "fault")} | {"total": s["total"]}

def piles(r): return (r.get("data") or {}).get("list") or []

def main():
    print("== admin_login ==")
    login = request({"type":"admin_login", "data":{"username":"admin", "password":"123456"}})
    assert login.get("code") == 0
    admin = login["data"]["token"]
    user_login = request({"type":"login", "data":{"phone":"13800138001"}})
    assert user_login.get("code") == 0
    user = user_login["data"]["token"]

    def pile(data, token=admin):
        return request({"type":"admin_pile_list", "token":token, "data":data})

    no_token = request({"type":"admin_pile_list", "data":{}})
    assert no_token.get("code") == 9
    non_admin = pile({}, user)
    assert non_admin.get("code") == 9

    base = pile({})
    assert base.get("code") == 0 and piles(base)
    base_stats = stats_shape(base)
    sample = piles(base)[0]
    real_code = sample["code"]
    code_part = real_code[:max(1, len(real_code)//2)]
    station_id = int(db("SELECT station_id FROM pile ORDER BY id LIMIT 1"))
    row = db("SELECT type,status FROM pile ORDER BY id LIMIT 1").split("\t")
    real_type, real_status = row[0], row[1]

    for name, data, pred in [
        ("status", {"status":"idle"}, lambda x:x.get("status")=="idle"),
        ("type", {"type":"fast"}, lambda x:x.get("type")=="fast"),
        ("station_id", {"station_id":station_id}, lambda x:True),
        ("code", {"code":code_part}, lambda x:code_part.lower() in x.get("code","").lower()),
        ("组合", {"station_id":station_id,"type":real_type,"status":real_status,"code":code_part},
         lambda x: x.get("type")==real_type and x.get("status")==real_status and code_part.lower() in x.get("code","").lower()),
    ]:
        r = pile(data); assert r.get("code") == 0, name
        assert all(pred(x) for x in piles(r)), name
        assert stats_shape(r) == base_stats, name + " 改变了全局 stats"

    for bad in ({"status":"invalid"}, {"type":"invalid"}):
        r = pile(bad); assert r.get("code") == 2

    p1 = pile({"page":1,"page_size":2}); p2 = pile({"page":2,"page_size":2})
    for r, page in ((p1,1),(p2,2)):
        assert r.get("code") == 0 and (r.get("data") or {}).get("page") == page
        assert len(piles(r)) <= 2 and stats_shape(r) == base_stats
    d1, d2 = p1["data"], p2["data"]
    assert d1["page_size"] == d2["page_size"] == 2 and d1["total"] == d2["total"]
    filtered = pile({"status":"idle", "page":1, "page_size":2})
    assert filtered.get("code") == 0 and (filtered.get("data") or {}).get("total",-1) >= len(piles(filtered))
    assert stats_shape(filtered) == base_stats
    missing = pile({"station_id": 999999999})
    assert missing.get("code") == 4, "不存在的 station_id 应返回 code=4"

    by_station = pile({"station_id": station_id, "page_size": 50})
    assert by_station.get("code") == 0
    assert stats_shape(by_station) == base_stats, "按站过滤不应改变全局 stats"
    rows = piles(by_station)
    assert rows, "站内应有电桩"
    for x in rows:
        assert "order_no" in x and "last_online_at" in x
        assert "code" in x and "type" in x and "power_kw" in x and "status" in x
        sid = int(db(f"SELECT station_id FROM pile WHERE id={int(x['id'])}"))
        assert sid == station_id, "list 中混入了其他站的桩"
    print("ALL NO.38/NO.100 ASSERTIONS PASSED")

if __name__ == "__main__":
    try: main()
    except Exception as e:
        print("TEST FAILED:", e); sys.exit(1)
