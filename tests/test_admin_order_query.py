#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
管理端订单查询：admin_order_list / admin_order_detail

前置：MySQL + charging-server(9000) 已启动。
用法：python3 tests/test_admin_order_query.py
"""

import json
import socket
import struct
import sys

HOST, PORT = "127.0.0.1", 9000

passed = failed = 0


def _recv_n(s, n):
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("连接被对端关闭")
        buf += chunk
    return buf


def request(obj: dict) -> dict:
    payload = json.dumps(obj).encode("utf-8")
    frame = struct.pack(">I", len(payload)) + payload
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(frame)
        header = _recv_n(s, 4)
        (length,) = struct.unpack(">I", header)
        body = _recv_n(s, length)
    return json.loads(body.decode("utf-8"))


def check(name: str, cond: bool, detail: str = ""):
    global passed, failed
    if cond:
        passed += 1
        print(f"  [PASS] {name}")
    else:
        failed += 1
        print(f"  [FAIL] {name}  {detail}")


print("== admin_order_list / admin_order_detail ==")

no_token = request({"type": "admin_order_list", "data": {}})
check("无 token → code=9", no_token.get("code") == 9, str(no_token))

user = request({"type": "login", "data": {"phone": "13800138001"}})
check("用户登录成功", user.get("code") == 0, str(user))
user_token = (user.get("data") or {}).get("token", "")
user_on_admin = request({
    "type": "admin_order_list",
    "token": user_token,
    "data": {},
})
check("用户 token 调管理端 → code=9", user_on_admin.get("code") == 9, str(user_on_admin))

admin = request({"type": "admin_login", "data": {"username": "admin", "password": "123456"}})
check("管理员登录成功", admin.get("code") == 0, str(admin))
token = (admin.get("data") or {}).get("token", "")

listed = request({"type": "admin_order_list", "token": token, "data": {}})
data = listed.get("data") or {}
check("列表 code=0", listed.get("code") == 0, str(listed))
check("列表含 total/page/page_size/list",
      all(k in data for k in ("total", "page", "page_size", "list")),
      str(data.keys()))
check("默认至少有 schema 里的已结算单", data.get("total", 0) >= 4, str(data.get("total")))
check("默认 page=1 page_size=20",
      data.get("page") == 1 and data.get("page_size") == 20,
      str(data))

first = (data.get("list") or [{}])[0]
needed = {
    "id", "order_no", "user_id", "nickname", "phone",
    "station_id", "station_name", "pile_id", "pile_code",
    "status", "kwh", "duration_seconds", "unit_price", "amount",
    "created_at", "start_time", "end_time",
}
check("列表行含联表字段", needed.issubset(first.keys()), str(first.keys()))

settled = request({
    "type": "admin_order_list",
    "token": token,
    "data": {"status": "settled"},
})
sdata = settled.get("data") or {}
s_list = sdata.get("list") or []
check("status=settled 全是 settled",
      settled.get("code") == 0 and s_list and all(x.get("status") == "settled" for x in s_list),
      str(settled))

by_no = request({
    "type": "admin_order_list",
    "token": token,
    "data": {"order_no": "CD20260828001"},
})
bdata = by_no.get("data") or {}
b_list = bdata.get("list") or []
check("按订单号能筛到 CD20260828001",
      by_no.get("code") == 0 and any(x.get("order_no") == "CD20260828001" for x in b_list),
      str(by_no))

paged = request({
    "type": "admin_order_list",
    "token": token,
    "data": {"page": 1, "page_size": 2},
})
pdata = paged.get("data") or {}
check("page_size=2 时本页最多 2 条且 total 不变",
      paged.get("code") == 0
      and len(pdata.get("list") or []) <= 2
      and pdata.get("page_size") == 2
      and pdata.get("total", 0) >= 4,
      str(paged))

today = request({
    "type": "admin_order_list",
    "token": token,
    "data": {"start_time": "2026-08-28", "end_time": "2026-08-28", "page_size": 5},
})
tdata = today.get("data") or {}
check("按日筛选 2026-08-28 能查到单",
      today.get("code") == 0 and tdata.get("total", 0) >= 1,
      str(today))

bad_status = request({
    "type": "admin_order_list",
    "token": token,
    "data": {"status": "paid"},
})
check("非法 status → code=2", bad_status.get("code") == 2, str(bad_status))

detail = request({
    "type": "admin_order_detail",
    "token": token,
    "data": {"order_no": "CD20260828001"},
})
dd = detail.get("data") or {}
check("详情 code=0 且字段与列表同行一致",
      detail.get("code") == 0
      and dd.get("order_no") == "CD20260828001"
      and dd.get("station_name")
      and dd.get("pile_code")
      and "amount" in dd,
      str(detail))

missing = request({
    "type": "admin_order_detail",
    "token": token,
    "data": {"order_no": "NO_SUCH_ORDER"},
})
check("详情不存在 → code=4", missing.get("code") == 4, str(missing))

empty = request({
    "type": "admin_order_detail",
    "token": token,
    "data": {},
})
check("详情缺 order_no → code=2", empty.get("code") == 2, str(empty))

print(f"\n结果: {passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
