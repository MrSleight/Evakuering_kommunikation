
import serial
import json
import time

PORT = "COM5"  
BAUD = 115200

def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.2)

    print("Python server connected")

    last_heartbeat = 0

    try:
        while True:

            now = time.monotonic()

            # Keepalive only, not an application command.
            if now - last_heartbeat >= 2:
                ser.write(b'{"cmd":"hello"}\n')
                last_heartbeat = now

            line = ser.readline().decode(
                "utf-8",
                errors="replace"
            ).strip()

            if not line:
                continue

            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue

            if event.get("event") == "server_data":
                print(
                    "Node:",
                    event.get("from"),
                    "| Data:",
                    event.get("payload")
                )

    except KeyboardInterrupt:
        print("Server stopped")

    finally:
        ser.close()


if __name__ == "__main__":
    main()
