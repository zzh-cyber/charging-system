#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NO.78 端到端测试：unfinished_order（未完成订单查询）

验证点：
  1) 不带 token           -> code=9（会话无效）
  2) 登录拿 token，查询    -> code=0，返回 data.order（结构可解析）
  3) 伪造 data.user_id     -> 被忽略，身份仍取自会话（结果与不伪造一致）
  4) 造一条 pending_payment 订单 -> 能被查出（含 pending_payment 状态）
  5) 清理后再查            -> data.order 为空

用法：
  确保 MySQL 与 charging-server 已启动（端口 9000），然后：
    python3 tests/test_unfinished_order.py
"""

import json
import socket
import struct
import subprocess
import sys

HOST = "127.0.0.1"
PORT = 9000
PHONE = "13800138001"

DB = ["mysql", "-u", "charging_user", "-p123456", "charging_system", "-N", "-B", "-e"]


def db(sql: str) -> str:
    r = subprocess.run(DB + [sql], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"MySQL 失败: {r.stderr.strip()}")
    return r.stdout.strip()


def request(obj: dict) -> dict:
    """一次短连接发送一个请求并读回一个响应（4字节大端长度 + UTF-8 JSON）。"""
    payload = json.dumps(obj).encode("utf-8")
    frame = struct.pack(">I", len(payload)) + payload
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(frame)
        header = _recv_n(s, 4)
        (length,) = struct.unpack(">I", header)
        body = _recv_n(s, length)
    return json.loads(body.decode("utf-8"))


def _recv_n(s: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("连接被对端关闭")
        buf += chunk
    return buf


def login() -> str:
    resp = request({"type": "login", "data": {"phone": PHONE}})
    assert resp.get("code") == 0, f"登录失败: {resp}"
    token = resp["data"]["token"]
    assert token, "登录未返回 token"
    return token


passed = 0
failed = 0


def check(name: str, cond: bool, detail: str = ""):
    global passed, failed
    if cond:
        passed += 1
        print(f"  [PASS] {name}")
    else:
        failed += 1
        print(f"  [FAIL] {name}  {detail}")


def main():
    print("== NO.78 unfinished_order 测试 ==")

    # 1) 不带 token
    r = request({"type": "unfinished_order", "data": {}})
    check("无 token 返回 code=9", r.get("code") == 9, f"实际: {r}")

    # 2) 登录拿 token
    token = login()
    uid = int(db(f"SELECT id FROM `user` WHERE phone='{PHONE}'"))
    print(f"  用户 id = {uid}")

    # 先确保没有历史未完成单干扰（把该用户 reserved/charging/pending_payment 归档为 cancelled）
    db(f"UPDATE charge_order SET status='cancelled' "
       f"WHERE user_id={uid} AND status IN ('reserved','charging','pending_payment')")

    r = request({"type": "unfinished_order", "token": token, "data": {}})
    check("带 token 返回 code=0", r.get("code") == 0, f"实际: {r}")
    order = r.get("data", {}).get("order", {})
    check("无未完成单时 order 为空", order == {} or not order.get("order_no"),
          f"实际: {order}")

    # 3) 伪造 user_id 被忽略：构造另一用户的未完成单，当前会话不应查到它
    other = db("SELECT id FROM `user` WHERE phone='13800138002'")
    if other:
        other = int(other)
        db(f"UPDATE charge_order SET status='cancelled' "
           f"WHERE user_id={other} AND status IN ('reserved','charging','pending_payment')")
        db("INSERT INTO charge_order (order_no,user_id,station_id,pile_id,status,unit_price,reserve_time) "
           f"VALUES ('T78OTHER',{other},1,1,'reserved',1.20,NOW())")
        r = request({"type": "unfinished_order", "token": token,
                     "data": {"user_id": other}})
        o = r.get("data", {}).get("order", {})
        check("伪造 data.user_id 无效（查不到他人单）",
              not o.get("order_no"), f"实际: {o}")
        db("DELETE FROM charge_order WHERE order_no='T78OTHER'")

    # 4) 造一条 pending_payment 订单，应被查出
    db("INSERT INTO charge_order (order_no,user_id,station_id,pile_id,status,unit_price,reserve_time,start_time,end_time,kwh,amount) "
       f"VALUES ('T78PEND',{uid},1,1,'pending_payment',1.20,NOW(),NOW(),NOW(),10.0,12.00)")
    r = request({"type": "unfinished_order", "token": token, "data": {}})
    o = r.get("data", {}).get("order", {})
    check("能查出 pending_payment 未完成单",
          o.get("order_no") == "T78PEND" and o.get("status") == "pending_payment",
          f"实际: {o}")

    # 5) 清理后再查为空
    db("DELETE FROM charge_order WHERE order_no='T78PEND'")
    r = request({"type": "unfinished_order", "token": token, "data": {}})
    o = r.get("data", {}).get("order", {})
    check("清理后 order 为空", not o.get("order_no"), f"实际: {o}")

    print(f"\n结果: {passed} 通过, {failed} 失败")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
