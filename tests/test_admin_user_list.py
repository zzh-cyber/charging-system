#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NO.46 admin_user_list：参数化模糊搜索 + 分页。前置：charging-server(9000) 已启动。"""
import json, socket, struct, subprocess, sys

HOST, PORT = "127.0.0.1", 9000
DB = ["mysql", "-u", "charging_user", "-p123456", "charging_system", "-N", "-B", "-e"]


def db(sql):
    r = subprocess.run(DB + [sql], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr.strip())
    return r.stdout.strip()


def recv_n(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise ConnectionError("对端关闭连接")
        b += c
    return b


def request(obj):
    payload = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.sendall(struct.pack(">I", len(payload)) + payload)
        size = struct.unpack(">I", recv_n(s, 4))[0]
        return json.loads(recv_n(s, size).decode("utf-8"))


def main():
    login = request({"type": "admin_login", "data": {"username": "admin", "password": "123456"}})
    assert login.get("code") == 0, login
    admin = login["data"]["token"]
    user_login = request({"type": "login", "data": {"phone": "13800138001"}})
    assert user_login.get("code") == 0
    user = user_login["data"]["token"]

    def users(data, token=admin):
        return request({"type": "admin_user_list", "token": token, "data": data})

    no_token = request({"type": "admin_user_list", "data": {}})
    assert no_token.get("code") == 9, no_token
    as_user = users({}, user)
    assert as_user.get("code") == 9, as_user

    total = int(db("SELECT COUNT(*) FROM `user`"))
    assert total >= 1

    base = users({})
    assert base.get("code") == 0, base
    data = base["data"]
    assert data.get("page") == 1
    assert data.get("page_size") == 20
    assert data.get("total") == total
    assert isinstance(data.get("list"), list)
    assert len(data["list"]) == min(total, 20)
    sample = data["list"][0]
    for k in ("id", "phone", "nickname", "balance", "status", "created_at"):
        assert k in sample, k

    phone = sample["phone"]
    hit = users({"keyword": phone[-4:]})
    assert hit.get("code") == 0, hit
    phones = [x["phone"] for x in hit["data"]["list"]]
    assert phone in phones, phones
    assert hit["data"]["total"] == len(hit["data"]["list"])

    nick = sample["nickname"]
    if nick:
        nick_hit = users({"keyword": nick})
        assert nick_hit.get("code") == 0
        assert any(x["nickname"] == nick for x in nick_hit["data"]["list"])

    empty = users({"keyword": "no-such-user-zzz"})
    assert empty.get("code") == 0, empty
    assert empty["data"]["list"] == []
    assert empty["data"]["total"] == 0

    page1 = users({"page": 1, "page_size": 1})
    assert page1.get("code") == 0
    assert page1["data"]["page"] == 1
    assert page1["data"]["page_size"] == 1
    assert page1["data"]["total"] == total
    assert len(page1["data"]["list"]) == 1
    id1 = page1["data"]["list"][0]["id"]

    if total >= 2:
        page2 = users({"page": 2, "page_size": 1})
        assert page2.get("code") == 0
        assert len(page2["data"]["list"]) == 1
        assert page2["data"]["list"][0]["id"] != id1

    capped = users({"page_size": 99})
    assert capped.get("code") == 0
    assert capped["data"]["page_size"] == 50

    like_literal = users({"keyword": "%"})
    assert like_literal.get("code") == 0
    assert like_literal["data"]["total"] == 0

    print("admin_user_list ok")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("FAIL:", e)
        sys.exit(1)
