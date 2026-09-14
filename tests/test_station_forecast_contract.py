import json
import socket
import struct


HOST = "127.0.0.1"
PORT = 9000
PHONE = "13800138001"


def recv_n(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("connection closed while receiving frame")
        data += chunk
    return data


def request(sock, payload):
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    sock.sendall(struct.pack(">I", len(encoded)) + encoded)
    (size,) = struct.unpack(">I", recv_n(sock, 4))
    return json.loads(recv_n(sock, size).decode("utf-8"))


def main():
    with socket.create_connection((HOST, PORT), timeout=5) as sock:
        login = request(sock, {"type": "login", "data": {"phone": PHONE}})
        assert login.get("code") == 0, login
        token = login["data"]["token"]

        response = request(
            sock,
            {
                "type": "station_list",
                "token": token,
                "data": {"lat": 22.5431, "lng": 114.0579},
            },
        )
        assert response.get("code") == 0, response
        stations = response.get("data", {}).get("list", [])
        assert stations, response

        old_fields = {
            "id", "name", "address", "longitude", "latitude", "price",
            "total", "idle", "distance",
        }
        forecast_fields = {
            "forecast_idle_1h", "forecast_idle_6h", "forecast_idle_24h",
            "forecast_util_1h", "forecast_util_6h", "forecast_util_24h",
            "congestion", "recommend_score", "is_peak_1h",
        }
        previous_distance = None
        for station in stations:
            assert old_fields <= station.keys(), station
            assert forecast_fields <= station.keys(), station
            assert station["congestion"] in {"low", "mid", "high"}, station
            for key in ("forecast_util_1h", "forecast_util_6h", "forecast_util_24h"):
                assert 0 <= station[key] <= 100, station
            distance = station["distance"]
            if previous_distance is not None:
                assert previous_distance <= distance, stations
            previous_distance = distance


if __name__ == "__main__":
    main()
