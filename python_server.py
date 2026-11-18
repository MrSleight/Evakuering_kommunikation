
import serial
import json
import time

PORT = "COM3"
BAUD = 115200

def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.2)
    print("Python server connected")

    last_heartbeat = 0

    try:
        while True:
            now = time.monotonic()

            # Keepalive till gatewayen
            if now - last_heartbeat >= 2:
                ser.write(b'{"cmd":"hello"}\n')
                last_heartbeat = now

            raw = ser.readline()

            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()

            if not line:
                continue

            # Försök tolka raden som JSON
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                print("[ESP32 LOG]", line)
                continue

            # Ta emot mesh-data från gatewayen
            if event.get("event") == "server_data":
                sender = event.get("from")
                payload = event.get("payload")

                print("[SERVER DATA]")
                print("Node:", sender)
                print("Data:", payload)
                print("-------------------------")

            else:
                print("[ESP32 JSON]", event)

    except KeyboardInterrupt:
        print("Server stopped")

    finally:
        ser.close()


if __name__ == "__main__":
    main()
