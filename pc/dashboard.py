"""Live dashboard for the demo: shows the board's answer for every flick.

Reads the same USB serial line as logger.py (RES,... and RAW,... lines) and
shows the health %, the fault class, the 8 LED health bar, f1 and damping,
the last flick's waveform and a health history. It only listens, so the
board works exactly as without it (keys C, B, D, A on the keypad).

  py pc/dashboard.py --port COM4
Practice without a board:
  py pc/dashboard.py --demo
"""
import argparse
import queue
import random
import threading
import time
import tkinter as tk

from logger import open_serial, parse_lines

BG, PANEL, TEXT, MUTED = "#101418", "#1b2128", "#e8edf2", "#8a96a3"
GREEN, AMBER, RED, BLUE = "#3ccf7a", "#f2b233", "#ef5350", "#4fa3f7"
NICE = {"healthy": "HEALTHY", "added_mass": "ADDED MASS (coin)",
        "loose_joint": "LOOSE JOINT", "loose_support": "LOOSE SUPPORT"}


def parse_res(res):
    """RES,nn,conf,tree,knn,health,mHz,damp*1e5,changed,unknown,unsafe"""
    p = res.split(",")
    if len(p) < 10:
        return None
    return {"nn": p[0], "conf": int(p[1]), "tree": p[2], "knn": p[3],
            "health": int(p[4]), "f1": int(p[5]) / 1000, "damp": int(p[6]) / 1e5,
            "changed": p[7] == "1", "unknown": p[8] == "1", "unsafe": p[9] == "1"}


def board_reader(port, baud, q):
    ser = open_serial(port, baud)
    for raw, res, _ in parse_lines(ser.readline().decode(errors="ignore") for _ in iter(int, 1)):
        q.put((raw, parse_res(res) if res else None))


def demo_reader(q):
    import math
    cases = [("healthy", 100, 4.30), ("added_mass", 85, 3.33), ("loose_joint", 50, 4.08),
             ("loose_joint", 0, 3.90), ("healthy", 100, 4.31)]
    while True:
        for cls, h, f in cases:
            for _ in range(3):
                time.sleep(2.5)
                f1 = f + random.uniform(-0.03, 0.03)
                raw = [int(2048 + 900 * math.exp(-0.18 * i / 50) * math.sin(2 * math.pi * f1 * i / 50))
                       for i in range(1024)]
                q.put((raw, {"nn": cls, "conf": 95, "tree": cls, "knn": cls, "health": h,
                             "f1": f1, "damp": 0.029, "changed": False,
                             "unknown": False, "unsafe": h < 40}))


class Dashboard:
    def __init__(self, root, q, source):
        self.q, self.hist, self.count = q, [], 0
        root.title("TinySHM live dashboard")
        root.configure(bg=BG)
        root.geometry("1100x680")
        tk.Label(root, text="TinySHM  |  vibration-based structural health monitor",
                 fg=TEXT, bg=BG, font=("Segoe UI", 18, "bold")).pack(anchor="w", padx=20, pady=(16, 0))
        self.status = tk.Label(root, text=f"Source: {source}  |  waiting for a flick...",
                               fg=MUTED, bg=BG, font=("Segoe UI", 11))
        self.status.pack(anchor="w", padx=20)

        top = tk.Frame(root, bg=BG)
        top.pack(fill="x", padx=20, pady=10)
        left = tk.Frame(top, bg=PANEL)
        left.pack(side="left", fill="both", expand=True, padx=(0, 10))
        self.health = tk.Label(left, text="--%", fg=TEXT, bg=PANEL, font=("Segoe UI", 64, "bold"))
        self.health.pack(pady=(14, 0))
        tk.Label(left, text="structural health", fg=MUTED, bg=PANEL, font=("Segoe UI", 11)).pack()
        self.cls = tk.Label(left, text="", fg=TEXT, bg=PANEL, font=("Segoe UI", 22, "bold"))
        self.cls.pack(pady=(10, 0))
        self.flag = tk.Label(left, text="", fg=MUTED, bg=PANEL, font=("Segoe UI", 13, "bold"))
        self.flag.pack(pady=(2, 10))
        self.leds = tk.Canvas(left, width=8 * 46, height=50, bg=PANEL, highlightthickness=0)
        self.leds.pack(pady=(0, 6))
        tk.Label(left, text="board LEDs (1 LED = 12.5%)", fg=MUTED, bg=PANEL,
                 font=("Segoe UI", 10)).pack(pady=(0, 12))

        right = tk.Frame(top, bg=PANEL)
        right.pack(side="left", fill="both", expand=True)
        self.stats = {}
        for key, name in [("f1", "Natural frequency f1"), ("damp", "Damping ratio"),
                          ("models", "Models (kNN / NN / tree)"), ("count", "Flicks this session")]:
            tk.Label(right, text=name, fg=MUTED, bg=PANEL, font=("Segoe UI", 11)).pack(anchor="w", padx=16, pady=(12, 0))
            self.stats[key] = tk.Label(right, text="--", fg=TEXT, bg=PANEL, font=("Segoe UI", 18, "bold"))
            self.stats[key].pack(anchor="w", padx=16)

        bottom = tk.Frame(root, bg=BG)
        bottom.pack(fill="both", expand=True, padx=20, pady=(0, 16))
        self.wave = self.chart(bottom, "Last flick (raw accelerometer)")
        self.trend = self.chart(bottom, "Health history")
        self.draw_leds(0)
        root.after(100, self.poll, root)

    def chart(self, parent, title):
        f = tk.Frame(parent, bg=PANEL)
        f.pack(side="left", fill="both", expand=True, padx=(0, 10))
        tk.Label(f, text=title, fg=MUTED, bg=PANEL, font=("Segoe UI", 11)).pack(anchor="w", padx=12, pady=(8, 0))
        c = tk.Canvas(f, bg=PANEL, highlightthickness=0, height=200)
        c.pack(fill="both", expand=True, padx=8, pady=8)
        return c

    def draw_leds(self, health, alarm=None):
        n = int(health / 12.5 + 0.5)
        self.leds.delete("all")
        col = GREEN if health >= 70 else AMBER if health >= 40 else RED
        for i in range(8):
            on = i < n
            x = 23 + i * 46
            self.leds.create_oval(x - 16, 9, x + 16, 41, fill=col if on else "#2c343d",
                                  outline=TEXT if alarm else "#3a444f", width=2 if alarm else 1)

    def line(self, c, ys, lo, hi, color):
        c.delete("all")
        w, h = max(c.winfo_width(), 50), max(c.winfo_height(), 50)
        if len(ys) < 2:
            return
        pts = []
        for i, y in enumerate(ys):
            pts += [6 + i * (w - 12) / (len(ys) - 1), h - 6 - (y - lo) / ((hi - lo) or 1) * (h - 12)]
        c.create_line(*pts, fill=color, width=2)

    def poll(self, root):
        try:
            while True:
                self.show(*self.q.get_nowait())
        except queue.Empty:
            pass
        root.after(100, self.poll, root)

    def show(self, raw, r):
        self.count += 1
        self.stats["count"].config(text=str(self.count))
        self.line(self.wave, list(raw), min(raw), max(raw), BLUE)
        if r is None:
            self.status.config(text="Flick received, no answer from the board (calibrating? press C and give 10 flicks)")
            return
        h = r["health"]
        col = GREEN if h >= 70 else AMBER if h >= 40 else RED
        self.health.config(text=f"{h}%", fg=col)
        self.cls.config(text=NICE.get(r["knn"], r["knn"].upper()))
        flag = ("UNSAFE" if r["unsafe"] else "UNKNOWN CHANGE" if r["unknown"]
                else "CHANGED / MARKED" if r["changed"] else "OK")
        self.flag.config(text=flag, fg=RED if r["unsafe"] else AMBER if flag != "OK" else GREEN)
        self.draw_leds(h, alarm=r["unsafe"] or r["unknown"])
        self.stats["f1"].config(text=f"{r['f1']:.2f} Hz")
        self.stats["damp"].config(text=f"{r['damp'] * 100:.2f} %")
        self.stats["models"].config(text=f"{r['knn']} / {r['nn']} ({r['conf']}%) / {r['tree']}")
        self.hist = (self.hist + [h])[-40:]
        self.line(self.trend, self.hist, 0, 100, col)
        self.status.config(text=f"Last flick at {time.strftime('%H:%M:%S')}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="e.g. COM4")
    ap.add_argument("--baud", type=int, default=19200)
    ap.add_argument("--demo", action="store_true", help="fake flicks, no board needed")
    a = ap.parse_args()
    if not a.demo and not a.port:
        ap.error("give --port COMx, or --demo")
    q = queue.Queue()
    target, args = (demo_reader, (q,)) if a.demo else (board_reader, (a.port, a.baud, q))
    threading.Thread(target=target, args=args, daemon=True).start()
    root = tk.Tk()
    Dashboard(root, q, "demo" if a.demo else a.port)
    root.mainloop()


if __name__ == "__main__":
    main()
