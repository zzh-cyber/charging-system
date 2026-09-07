#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NO.40/98 admin_pile_restart：充电中拒绝；成功写指令与审计。前置：charging-server(9000)。"""
import json, socket, struct, subprocess, sys, uuid

HOST, PORT = "127.0.0.1", 9000
DB = ["mysql", "-u", "charging_user", "-p123456", "charging_system", "-N", "-B", "-e"]


def db(sql):
    r = subprocess.run(DB + [sql], capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr.strip() or sql)
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
    admin_id = int(login["data"]["id"])
    user_login = request({"type": "login", "data": {"phone": "13800138001"}})
    assert user_login.get("code") == 0
    user = user_login["data"]["token"]

    def restart(pile_id, token=admin):
        return request({"type": "admin_pile_restart", "token": token,
                        "data": {"pile_id": pile_id}})

    no_token = request({"type": "admin_pile_restart", "data": {"pile_id": 1}})
    assert no_token.get("code") == 9
    as_user = restart(1, user)
    assert as_user.get("code") == 9

    bad = restart(0)
    assert bad.get("code") == 2, bad
    missing = restart(99999999)
    assert missing.get("code") == 4, missing

    row = db(
        "SELECT id, status FROM pile WHERE id NOT IN ("
        "  SELECT pile_id FROM charge_order WHERE status='charging')"
        " ORDER BY CASE status WHEN 'fault' THEN 0 ELSE 1 END, id LIMIT 1"
    )
    pile_id, old_status = row.split("\t")
    pile_id = int(pile_id)

    ok = restart(pile_id)
    assert ok.get("code") == 0, ok
    assert ok["data"]["id"] == pile_id
    assert ok["data"]["status"] == "idle"
    command_no = ok["data"]["command_no"]
    assert command_no
    assert db(f"SELECT status FROM pile WHERE id={pile_id}") == "idle"
    cmd = db(
        f"SELECT pile_id, command, status FROM device_commands "
        f"WHERE command_no='{command_no}'"
    )
    assert cmd == f"{pile_id}\trestart\tsuccess", cmd
    assert db(
        f"SELECT response_at IS NOT NULL FROM device_commands WHERE command_no='{command_no}'"
    ) == "1"
    log = db(
        f"SELECT admin_id, action, target_type, target_id, before_value, after_value "
        f"FROM operation_logs WHERE target_type='pile' AND target_id={pile_id} "
        f"ORDER BY id DESC LIMIT 1"
    )
    assert log == f"{admin_id}\tpile_restart\tpile\t{pile_id}\t{old_status}\tidle", log

    db(f"UPDATE pile SET status='{old_status}' WHERE id={pile_id}")

    idle_row = db(
        "SELECT p.id, p.station_id FROM pile p "
        "WHERE p.status='idle' AND NOT EXISTS ("
        "  SELECT 1 FROM charge_order o WHERE o.pile_id=p.id "
        "  AND o.status IN ('reserved','charging')) "
        "ORDER BY p.id LIMIT 1"
    )
    idle_id, station_id = idle_row.split("\t")
    idle_id, station_id = int(idle_id), int(station_id)
    user_id = int(db("SELECT id FROM user WHERE phone='13800138001'"))
    order_no = "TEST-RST-" + uuid.uuid4().hex[:16]
    db(
        "INSERT INTO charge_order (order_no, user_id, station_id, pile_id, status, unit_price) "
        f"VALUES ('{order_no}', {user_id}, {station_id}, {idle_id}, 'charging', 1.20)"
    )
    cmd_before = int(db(f"SELECT COUNT(*) FROM device_commands WHERE pile_id={idle_id}"))
    try:
        blocked = restart(idle_id)
        assert blocked.get("code") == 2, blocked
        assert "充电" in blocked.get("msg", "")
        assert db(f"SELECT status FROM pile WHERE id={idle_id}") == "idle"
        cmd_after = int(db(f"SELECT COUNT(*) FROM device_commands WHERE pile_id={idle_id}"))
        assert cmd_after == cmd_before, (cmd_before, cmd_after)
    finally:
        db(f"DELETE FROM charge_order WHERE order_no='{order_no}'")

    print("admin_pile_restart ok")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("FAIL:", e)
        sys.exit(1)
