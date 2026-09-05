#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NO.62 会话与安全 端到端并发/安全测试

覆盖：
  1) 无 token / 无效 token           -> code=9
  2) 伪造 data.user_id 无效           -> 充值记到会话用户，被伪造的目标用户余额不变
  3) 冻结用户后其旧 token 立即失效     -> revokeByUser 生效，下个请求 code=9
  4) 双预约仅一人成功（并发同一 idle 桩）
  5) 同一 token 高并发只读请求全部成功、服务端不崩

前置：MySQL + charging-server(9000) 已启动。
用法：python3 tests/test_session_security.py
"""

import json
import socket
import struct
import subprocess
import sys
import threading

HOST, PORT = "127.0.0.1", 9000
PHONE_A = "13800138001"
PHONE_B = "13800138002"
DB = ["mysql", "-u", "charging_user", "-p123456", "charging_system", "-N", "-B", "-e"]

passed = failed = 0


def db(sql: str) -> str:
    r = subprocess.run(DB + [sql], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"MySQL 失败: {r.stderr.strip()}")
    return r.stdout.strip()


def _recv_n(s, n):
    buf = b""
    while len(buf) < n:
        c = s.recv(n - len(buf))
        if not c:
            raise ConnectionError("对端关闭")
        buf += c
    return buf


def request(obj: dict) -> dict:
    payload = json.dumps(obj).encode("utf-8")
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(struct.pack(">I", len(payload)) + payload)
        (length,) = struct.unpack(">I", _recv_n(s, 4))
        body = _recv_n(s, length)
    return json.loads(body.decode("utf-8"))


def login(phone: str) -> str:
    r = request({"type": "login", "data": {"phone": phone}})
    assert r.get("code") == 0, f"登录失败({phone}): {r}"
    return r["data"]["token"]


def admin_login() -> str:
    r = request({"type": "admin_login",
                 "data": {"username": "admin", "password": "123456"}})
    assert r.get("code") == 0, f"管理员登录失败: {r}"
    return r["data"]["token"]


def check(name, cond, detail=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"  [PASS] {name}")
    else:
        failed += 1
        print(f"  [FAIL] {name}  {detail}")


def uid(phone: str) -> int:
    return int(db(f"SELECT id FROM `user` WHERE phone='{phone}'"))


def balance(user_id: int) -> float:
    return float(db(f"SELECT balance FROM `user` WHERE id={user_id}"))


def clear_unfinished(user_id: int):
    db(f"UPDATE charge_order SET status='cancelled' "
       f"WHERE user_id={user_id} AND status IN ('reserved','charging','pending_payment')")


def main():
    print("== NO.62 会话与安全测试 ==")
    ida, idb = uid(PHONE_A), uid(PHONE_B)
    # 保证两个测试用户都是 normal
    db(f"UPDATE `user` SET status='normal' WHERE id IN ({ida},{idb})")

    # ---- 1) 无/无效 token ----
    r = request({"type": "recharge", "data": {"amount": 1}})
    check("无 token 充值 code=9", r.get("code") == 9, f"{r}")
    r = request({"type": "recharge", "token": "not-a-real-token",
                 "data": {"amount": 1}})
    check("无效 token 充值 code=9", r.get("code") == 9, f"{r}")

    # ---- 2) 伪造 data.user_id 无效 ----
    tokenA = login(PHONE_A)
    balA0, balB0 = balance(ida), balance(idb)
    r = request({"type": "recharge", "token": tokenA,
                 "data": {"amount": 7.0, "user_id": idb}})  # 伪造成给 B 充
    check("伪造 user_id 充值 code=0", r.get("code") == 0, f"{r}")
    balA1, balB1 = balance(ida), balance(idb)
    check("充值记到会话用户 A（A+7）", abs(balA1 - (balA0 + 7.0)) < 1e-6,
          f"A {balA0}->{balA1}")
    check("被伪造的 B 余额不变", abs(balB1 - balB0) < 1e-6, f"B {balB0}->{balB1}")

    # ---- 5) 同一 token 高并发只读（放冻结之前，避免 token 被吊销）----
    results = []
    lock = threading.Lock()

    def hammer():
        rr = request({"type": "unfinished_order", "token": tokenA, "data": {}})
        with lock:
            results.append(rr.get("code"))

    threads = [threading.Thread(target=hammer) for _ in range(20)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    check("同 token 并发 20 次全部 code=0",
          len(results) == 20 and all(c == 0 for c in results),
          f"结果: {results}")

    # ---- 4) 双预约仅一人成功（并发同一 idle 桩）----
    clear_unfinished(ida)
    clear_unfinished(idb)
    pile_id = int(db("SELECT id FROM pile WHERE status='idle' ORDER BY id LIMIT 1"))
    db(f"UPDATE pile SET status='idle', current_user_id=NULL WHERE id={pile_id}")
    tokenA2 = login(PHONE_A)
    tokenB = login(PHONE_B)
    codes = {}

    def reserve(tag, tok):
        rr = request({"type": "reserve", "token": tok,
                      "data": {"pile_id": pile_id}})
        with lock:
            codes[tag] = rr.get("code")

    ta = threading.Thread(target=reserve, args=("A", tokenA2))
    tb = threading.Thread(target=reserve, args=("B", tokenB))
    ta.start(); tb.start(); ta.join(); tb.join()
    ok_count = sum(1 for c in codes.values() if c == 0)
    order_count = int(db(
        f"SELECT COUNT(*) FROM charge_order WHERE pile_id={pile_id} "
        f"AND status='reserved'"))
    check("双预约恰好一人成功", ok_count == 1, f"codes={codes}")
    check("同一桩只产生一条预约订单", order_count == 1, f"订单数={order_count}")
    # 清理预约测试
    clear_unfinished(ida)
    clear_unfinished(idb)
    db(f"UPDATE pile SET status='idle', current_user_id=NULL WHERE id={pile_id}")

    # ---- 3) 冻结后旧 token 立即失效 ----
    tokenA3 = login(PHONE_A)
    # 确认冻结前可用
    r = request({"type": "unfinished_order", "token": tokenA3, "data": {}})
    check("冻结前 A 的 token 可用", r.get("code") == 0, f"{r}")
    admin = admin_login()
    r = request({"type": "admin_user_freeze", "token": admin,
                 "data": {"user_id": ida, "frozen": True}})
    check("管理员冻结 A code=0", r.get("code") == 0, f"{r}")
    r = request({"type": "unfinished_order", "token": tokenA3, "data": {}})
    check("冻结后 A 旧 token 立即失效 code=9", r.get("code") == 9, f"{r}")
    # 解冻恢复
    request({"type": "admin_user_freeze", "token": admin,
             "data": {"user_id": ida, "frozen": False}})
    db(f"UPDATE `user` SET status='normal' WHERE id={ida}")

    print(f"\n结果: {passed} 通过, {failed} 失败")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
