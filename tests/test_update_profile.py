#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NO.75/76 update_profile：改头像/昵称，身份取自会话。"""
import json, socket, struct, subprocess, sys

HOST, PORT = "127.0.0.1", 9000
PHONE = "13800138001"
DB = ["mysql", "-u", "charging_user", "-p123456", "charging_system", "-N", "-B", "-e"]


def db(sql):
    r = subprocess.run(DB + [sql], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr.strip())
    return r.stdout.rstrip("\n")


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
    uid = int(db(f"SELECT id FROM user WHERE phone='{PHONE}'"))
    other = int(db(f"SELECT id FROM user WHERE phone<>'{PHONE}' ORDER BY id LIMIT 1"))
    old_nick, old_avatar = db(f"SELECT nickname, avatar FROM user WHERE id={uid}").split("\t")
    if old_avatar == "NULL":
        old_avatar = ""
    other_avatar = db(f"SELECT avatar FROM user WHERE id={other}")
    if other_avatar == "NULL":
        other_avatar = ""

    login = request({"type": "login", "data": {"phone": PHONE}})
    assert login.get("code") == 0, login
    token = login["data"]["token"]
    admin = request({"type": "admin_login", "data": {"username": "admin", "password": "123456"}})
    assert admin.get("code") == 0
    admin_token = admin["data"]["token"]

    def upd(data, tok=token):
        return request({"type": "update_profile", "token": tok, "data": data})

    try:
        no_token = request({"type": "update_profile", "data": {"avatar": "x"}})
        assert no_token.get("code") == 9

        as_admin = upd({"avatar": "x"}, admin_token)
        assert as_admin.get("code") == 9

        empty = upd({})
        assert empty.get("code") == 2

        too_long = upd({"avatar": "a" * 256})
        assert too_long.get("code") == 2

        not_str = upd({"avatar": 123})
        assert not_str.get("code") == 2

        new_avatar = "avatars/user_%d.png" % uid
        forged = upd({"avatar": new_avatar, "user_id": other})
        assert forged.get("code") == 0, forged
        assert forged.get("data", {}).get("avatar") == new_avatar
        assert "nickname" not in (forged.get("data") or {})
        assert db(f"SELECT avatar FROM user WHERE id={uid}") == new_avatar
        got_other = db(f"SELECT avatar FROM user WHERE id={other}")
        if got_other == "NULL":
            got_other = ""
        assert got_other == other_avatar, "伪造 user_id 改到了别人头像"

        relogin = request({"type": "login", "data": {"phone": PHONE}})
        assert relogin.get("code") == 0
        assert relogin["data"]["avatar"] == new_avatar

        nick_only = upd({"nickname": old_nick})
        assert nick_only.get("code") == 0
        assert nick_only["data"]["nickname"] == old_nick
        assert "avatar" not in nick_only["data"]

        both = upd({"nickname": old_nick, "avatar": new_avatar})
        assert both.get("code") == 0
        assert both["data"]["nickname"] == old_nick
        assert both["data"]["avatar"] == new_avatar
        print("ALL NO.75 ASSERTIONS PASSED")
    finally:
        db(f"UPDATE user SET nickname='{old_nick}', avatar='{old_avatar}' WHERE id={uid}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("TEST FAILED:", e)
        sys.exit(1)
