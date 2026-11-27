
import serial
import json
import time
import tkinter as tk
from tkinter import scrolledtext
from PIL import Image, ImageTk
import threading

PORT = "COM3"
BAUD = 115200

MAP_IMAGE = "node_map.png"


# ---------------- NODE CONFIGURATION ----------------

# Coordinates are based on the ORIGINAL PNG image.
# Change x and y to position your nodes.

nodes = [
    {"name": "Node 1", "x": 900, "y": 300, "status": "offline"},
    {"name": "Node 2", "x": 900, "y": 665, "status": "offline"},
    {"name": "Node 3", "x": 900, "y": 1000, "status": "offline"}
]


def main():
    log_file = open("server_log.txt", "a", encoding="utf-8")

    # ---------------- GUI SETUP ----------------

    window = tk.Tk()
    window.title("ESP32 Mesh Server")
    window.geometry("1200x700")

    stop_event = threading.Event()

    window.grid_columnconfigure(0, weight=1)
    window.grid_columnconfigure(1, weight=1)
    window.grid_rowconfigure(0, weight=1)

    # ---------------- LEFT PANEL ----------------

    left_frame = tk.Frame(window, padx=10, pady=10)
    left_frame.grid(row=0, column=0, sticky="nsew")

    left_frame.grid_columnconfigure(0, weight=1)

    # Terminal and buttons get equal vertical space
    left_frame.grid_rowconfigure(1, weight=1)
    left_frame.grid_rowconfigure(2, weight=1)

    # Header
    header_frame = tk.Frame(left_frame)
    header_frame.grid(row=0, column=0, sticky="ew")

    tk.Label(
        header_frame,
        text="SERVER TERMINAL",
        font=("Arial", 15, "bold")
    ).pack(side="left", pady=10)

    status_label = tk.Label(
        header_frame,
        text="Connecting...",
        fg="orange",
        font=("Arial", 11)
    )
    status_label.pack(side="right", padx=10)

    # Terminal
    message_area = scrolledtext.ScrolledText(
        left_frame,
        font=("Consolas", 10),
        bg="#1e1e1e",
        fg="#ffffff",
        insertbackground="white",
        state="disabled"
    )

    message_area.grid(
        row=1,
        column=0,
        sticky="nsew"
    )

    def update_log(message):
        message_area.config(state="normal")
        message_area.insert(tk.END, message + "\n")
        message_area.see(tk.END)
        message_area.config(state="disabled")

    def log(message):
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        log_file.write(f"[{timestamp}] {message}\n")
        log_file.flush()
        window.after(0, update_log, message)

    # ---------------- BUTTON PANEL ----------------

    button_frame = tk.Frame(
        left_frame,
        padx=5,
        pady=15
    )

    button_frame.grid(
        row=2,
        column=0,
        sticky="nsew"
    )

    button_frame.grid_columnconfigure(0, weight=1)
    button_frame.grid_columnconfigure(1, weight=1)
    button_frame.grid_columnconfigure(2, weight=1)
    button_frame.grid_rowconfigure(0, weight=1)
    button_frame.grid_rowconfigure(1, weight=1)
    button_frame.grid_rowconfigure(2, weight=1)


    def button_clicked(number):
        if number == 1:
            log("Button pressed: Left")
        elif number == 2:
            log("Button pressed: Middle")
        elif number == 3:
            log("Button pressed: Right")
        elif number == 4:
            log("People detected: Left")
        elif number == 5:
            log("People detected: Middle")
        elif number == 6:
            log("People detected: Right")
        elif number == 7:
            log("Someone is in: Left")
        elif number == 8:
            log("Someone is in: Middle")
        elif number == 9:
            log("Someone is in: Right")

    tk.Button(button_frame,text=f"Vänster Knapp",font=("Arial",12),command=lambda n=1 : button_clicked(1)).grid(row=0//3,column=0%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"Mitten Knapp",font=("Arial",12),command=lambda n=2 : button_clicked(2)).grid(row=1//3,column=1%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"Höger Knapp",font=("Arial",12),command=lambda n=3 : button_clicked(3)).grid(row=2//3,column=2%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"X Personer Vänster",font=("Arial",12),command=lambda n=4 : button_clicked(4)).grid(row=3//3,column=3%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"X Personer Mitten",font=("Arial",12),command=lambda n=5 : button_clicked(5)).grid(row=4//3,column=4%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"X Personer Höger",font=("Arial",12),command=lambda n=6 : button_clicked(6)).grid(row=5//3,column=5%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"Person Detekterad Vänster",font=("Arial",12),command=lambda n=7 : button_clicked(7)).grid(row=6//3,column=6%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"Person Detekterad Mitten",font=("Arial",12),command=lambda n=8 : button_clicked(8)).grid(row=7//3,column=7%3,sticky="nsew",padx=5,pady=5)
    tk.Button(button_frame,text=f"Person Detekterad Höger",font=("Arial",12),command=lambda n=9 : button_clicked(9)).grid(row=8//3,column=8%3,sticky="nsew",padx=5,pady=5)
    

    # ---------------- RIGHT PANEL: NODE MAP ----------------

    right_frame = tk.Frame(window, padx=10, pady=10)
    right_frame.grid(row=0, column=1, sticky="nsew")

    right_frame.grid_rowconfigure(1, weight=1)
    right_frame.grid_columnconfigure(0, weight=1)

    tk.Label(
        right_frame,
        text="MESH NETWORK MAP",
        font=("Arial", 15, "bold")
    ).grid(row=0, column=0, pady=10)

    # Canvas is the drawing surface
    map_canvas = tk.Canvas(
        right_frame,
        bg="#eeeeee",
        highlightthickness=0
    )

    map_canvas.grid(
        row=1,
        column=0,
        sticky="nsew"
    )

    # Load original PNG
    original_image = Image.open(MAP_IMAGE)

    # Keep a reference to the displayed image
    map_photo = None

    # ---------------- DRAW MAP AND NODES ----------------

    def draw_map(event=None):

        nonlocal map_photo

        width = map_canvas.winfo_width()
        height = map_canvas.winfo_height()

        if width <= 1 or height <= 1:
            return

        # Calculate scale while keeping image proportions
        scale = min(
            width / original_image.width,
            height / original_image.height
        )

        new_width = int(original_image.width * scale)
        new_height = int(original_image.height * scale)

        # Resize PNG
        resized_image = original_image.resize(
            (new_width, new_height),
            Image.Resampling.LANCZOS
        )

        map_photo = ImageTk.PhotoImage(resized_image)

        # Center image inside Canvas
        offset_x = (width - new_width) / 2
        offset_y = (height - new_height) / 2

        map_canvas.delete("all")

        # Draw background image
        map_canvas.create_image(
            offset_x,
            offset_y,
            image=map_photo,
            anchor="nw"
        )

        # Draw all nodes
        for node in nodes:

            # Convert original PNG coordinates to Canvas coordinates
            x = offset_x + node["x"] * scale
            y = offset_y + node["y"] * scale

            # Static node colors
            if node["status"] == "online":
                color = "green"
            else:
                color = "red"

            radius = max(6, 10 * scale)

            # Draw node circle
            map_canvas.create_oval(
                x - radius,
                y - radius,
                x + radius,
                y + radius,
                fill=color,
                outline="black",
                width=2
            )

            # Draw node name
            map_canvas.create_text(
                x + radius + 5,
                y,
                text=node["name"],
                anchor="w",
                fill="black",
                font=("Arial", 10, "bold")
            )

    # Redraw map when window size changes
    map_canvas.bind("<Configure>", draw_map)

    # ---------------- SERVER COMMUNICATION ----------------

    def server():

        try:
            ser = serial.Serial(PORT, BAUD, timeout=0.2)

            log("Python server connected")

            window.after(
                0,
                lambda: status_label.config(
                    text="Connected",
                    fg="green"
                )
            )

            last_heartbeat = 0

            try:
                while not stop_event.is_set():

                    now = time.monotonic()

                    # Keepalive till gatewayen
                    if now - last_heartbeat >= 2:
                        ser.write(b'{"cmd":"hello"}\n')
                        last_heartbeat = now

                    raw = ser.readline()

                    if not raw:
                        continue

                    line = raw.decode(
                        "utf-8",
                        errors="replace"
                    ).strip()

                    if not line:
                        continue

                    # Försök tolka raden som JSON
                    try:
                        event = json.loads(line)

                    except json.JSONDecodeError:
                        log("[ESP32 LOG] " + line)
                        continue

                    # Ta emot mesh-data från gatewayen
                    if event.get("event") == "server_data":

                        sender = event.get("from")
                        payload = event.get("payload")

                        log("[SERVER DATA]")
                        log("Node: " + str(sender))
                        log("Data: " + str(payload))
                        log("-------------------------")

                    else:
                        log("[ESP32 JSON] " + str(event))

            finally:
                ser.close()

        except serial.SerialException as e:

            log("Serial connection error: " + str(e))

            if not stop_event.is_set():
                window.after(
                    0,
                    lambda: status_label.config(
                        text="Disconnected",
                        fg="red"
                    )
                )

    # ---------------- START SERVER THREAD ----------------

    server_thread = threading.Thread(
        target=server,
        daemon=True
    )

    server_thread.start()

    # ---------------- CLOSE WINDOW ----------------

    def close_window():
        stop_event.set()
        log_file.close()
        window.destroy()

    window.protocol("WM_DELETE_WINDOW", close_window)

    window.mainloop()


if __name__ == "__main__":
    main()
