"""Live dashboard for the demo: shows the board's answer for every flick.

Reads the same USB serial line as logger.py (RES,... and RAW,... lines) and
shows the health gauge, the fault class with what it means, the 3 models'
votes, f1 and damping against the healthy baseline, the waveform, its
spectrum, a health history and an event log. It only listens, so the board
works exactly as without it (keys C, B, D, A, F on the keypad).

  py pc/dashboard.py --port COM4
Practice without a board:
  py pc/dashboard.py --demo
"""
import argparse
import math
import queue
import random
import statistics
import threading
import time
import tkinter as tk

import numpy as np

from config import FS
from logger import open_serial, parse_lines

BG, CARD, EDGE, GRID = "#0d1117", "#161b22", "#30363d", "#21262d"
TEXT, MUTED, DIM = "#e6edf3", "#8b949e", "#484f58"
GREEN, AMBER, RED, BLUE, PURPLE = "#3fb950", "#d29922", "#f85149", "#58a6ff", "#bc8cff"
FONT = "Segoe UI"

INFO = {
    "healthy": ("HEALTHY", GREEN,
                "Beam vibrates like the calibrated baseline.",
                "No action needed."),
    "added_mass": ("ADDED MASS", BLUE,
                   "Extra weight on the beam (the coin). Frequency drops because mass "
                   "goes up, but stiffness is intact.",
                   "Safe load. Remove the weight if it is not meant to be there."),
    "loose_joint": ("LOOSE JOINT", AMBER,
                    "Binder clips missing between the rulers. The joint is less stiff, "
                    "so the beam rings lower and loses energy faster.",
                    "Inspect and re-tighten the joint."),
    "loose_support": ("LOOSE SUPPORT", PURPLE,
                      "Clamp at the table edge is loose (marked by the operator with key B).",
                      "Tighten the clamp, then press D."),
}


def health_color(h):
    return GREEN if h >= 70 else AMBER if h >= 40 else RED


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
    cases = [("healthy", 100, 4.30, 0.029), ("added_mass", 85, 3.33, 0.027),
             ("loose_joint", 50, 4.08, 0.036), ("loose_joint", 0, 3.90, 0.045),
             ("healthy", 100, 4.31, 0.029)]
    while True:
        for cls, h, f, z in cases:
            for _ in range(3):
                time.sleep(2.5)
                f1 = f + random.uniform(-0.03, 0.03)
                w = 2 * math.pi * f1
                raw = [int(2048 + 900 * math.exp(-z * w * i / FS) * math.sin(w * i / FS)
                           + random.gauss(0, 8)) for i in range(1024)]
                q.put((raw, {"nn": cls, "conf": random.randint(88, 99), "tree": cls, "knn": cls,
                             "health": h, "f1": f1, "damp": z, "changed": False,
                             "unknown": False, "unsafe": h < 40}))


class Card(tk.Frame):
    def __init__(self, parent, title):
        super().__init__(parent, bg=CARD, highlightbackground=EDGE, highlightthickness=1)
        tk.Label(self, text=title.upper(), fg=MUTED, bg=CARD,
                 font=(FONT, 9, "bold")).pack(anchor="w", padx=14, pady=(10, 2))


class Dashboard:
    def __init__(self, root, q, source):
        self.root, self.q, self.source = root, q, source
        self.hist, self.events, self.healthy_f1, self.healthy_z = [], [], [], []
        self.raw, self.r, self.count = None, None, 0
        root.title("TinySHM live dashboard")
        root.configure(bg=BG)
        root.geometry("1400x860")
        root.minsize(1100, 720)

        # header
        head = tk.Frame(root, bg=BG)
        head.pack(fill="x", padx=18, pady=(14, 6))
        tk.Label(head, text="TinySHM", fg=TEXT, bg=BG, font=(FONT, 22, "bold")).pack(side="left")
        tk.Label(head, text="   Vibration-based structural health monitor  |  LPC1768 + MPU6050  |  on-board ML",
                 fg=MUTED, bg=BG, font=(FONT, 11)).pack(side="left", pady=(8, 0))
        self.clock = tk.Label(head, text="", fg=MUTED, bg=BG, font=(FONT, 11))
        self.clock.pack(side="right", padx=(10, 0))
        self.pill = tk.Label(head, text=("  DEMO  " if source == "demo" else f"  LIVE  {source}  "),
                             fg=BG, bg=AMBER if source == "demo" else GREEN, font=(FONT, 10, "bold"))
        self.pill.pack(side="right")

        body = tk.Frame(root, bg=BG)
        body.pack(fill="both", expand=True, padx=12)
        for c, w in enumerate((3, 4, 3)):
            body.columnconfigure(c, weight=w, uniform="col")
        body.rowconfigure(0, weight=6)
        body.rowconfigure(1, weight=4)
        body.rowconfigure(2, weight=4)

        # row 0: gauge | condition + models | metrics
        g = Card(body, "Structural health")
        g.grid(row=0, column=0, sticky="nsew", padx=6, pady=6)
        self.gauge = tk.Canvas(g, bg=CARD, highlightthickness=0, height=150)
        self.gauge.pack(fill="both", expand=True, padx=8)
        self.leds = tk.Canvas(g, bg=CARD, highlightthickness=0, height=40)
        self.leds.pack(fill="x", padx=8)
        tk.Label(g, text="Board LEDs  (1 LED = 12.5 %)", fg=DIM, bg=CARD,
                 font=(FONT, 9)).pack(pady=(0, 8))

        c = Card(body, "Condition")
        c.grid(row=0, column=1, sticky="nsew", padx=6, pady=6)
        top = tk.Frame(c, bg=CARD)
        top.pack(fill="x", padx=14)
        self.cls = tk.Label(top, text="Waiting for a flick", fg=TEXT, bg=CARD, font=(FONT, 24, "bold"))
        self.cls.pack(side="left")
        self.flag = tk.Label(top, text="", fg=BG, bg=CARD, font=(FONT, 10, "bold"))
        self.flag.pack(side="left", padx=12, pady=(6, 0))
        self.desc = tk.Label(c, text="Press C on the keypad and give 10 flicks to calibrate, then flick the beam.",
                             fg=TEXT, bg=CARD, font=(FONT, 11), justify="left", anchor="w", wraplength=480)
        self.desc.pack(fill="x", padx=14, pady=(6, 0))
        self.action = tk.Label(c, text="", fg=MUTED, bg=CARD, font=(FONT, 10, "italic"),
                               justify="left", anchor="w", wraplength=480)
        self.action.pack(fill="x", padx=14, pady=(4, 8))
        tk.Label(c, text="MODEL VOTES", fg=MUTED, bg=CARD, font=(FONT, 9, "bold")).pack(anchor="w", padx=14)
        votes = tk.Frame(c, bg=CARD)
        votes.pack(fill="x", padx=10, pady=(4, 10))
        self.votes = {}
        for i, (k, name) in enumerate([("knn", "kNN  (health)"), ("nn", "Neural net 2-8-4"),
                                       ("tree", "Decision tree")]):
            votes.columnconfigure(i, weight=1, uniform="v")
            box = tk.Frame(votes, bg=BG, highlightbackground=EDGE, highlightthickness=1)
            box.grid(row=0, column=i, sticky="nsew", padx=4)
            tk.Label(box, text=name, fg=MUTED, bg=BG, font=(FONT, 9)).pack(anchor="w", padx=8, pady=(6, 0))
            lab = tk.Label(box, text="--", fg=TEXT, bg=BG, font=(FONT, 12, "bold"))
            lab.pack(anchor="w", padx=8, pady=(0, 6))
            self.votes[k] = lab

        m = Card(body, "Vibration features vs baseline")
        m.grid(row=0, column=2, sticky="nsew", padx=6, pady=6)
        self.metric = {}
        for key, name in [("f1", "Natural frequency f1"), ("z", "Damping ratio  ζ"),
                          ("count", "Flicks this session")]:
            row = tk.Frame(m, bg=CARD)
            row.pack(fill="x", padx=14, pady=(6, 2))
            tk.Label(row, text=name, fg=MUTED, bg=CARD, font=(FONT, 10)).pack(anchor="w")
            line = tk.Frame(row, bg=CARD)
            line.pack(fill="x")
            big = tk.Label(line, text="--", fg=TEXT, bg=CARD, font=(FONT, 20, "bold"))
            big.pack(side="left")
            delta = tk.Label(line, text="", fg=MUTED, bg=CARD, font=(FONT, 11, "bold"))
            delta.pack(side="left", padx=10, pady=(8, 0))
            sub = tk.Label(row, text="", fg=DIM, bg=CARD, font=(FONT, 9))
            sub.pack(anchor="w")
            self.metric[key] = (big, delta, sub)

        # row 1: waveform | spectrum
        w = Card(body, "Last flick: acceleration vs time")
        w.grid(row=1, column=0, columnspan=2, sticky="nsew", padx=6, pady=6)
        self.wave = tk.Canvas(w, bg=CARD, highlightthickness=0)
        self.wave.pack(fill="both", expand=True, padx=6, pady=(0, 6))
        s = Card(body, "Spectrum (FFT)")
        s.grid(row=1, column=2, sticky="nsew", padx=6, pady=6)
        self.spec = tk.Canvas(s, bg=CARD, highlightthickness=0)
        self.spec.pack(fill="both", expand=True, padx=6, pady=(0, 6))

        # row 2: history | event log
        h = Card(body, "Health history")
        h.grid(row=2, column=0, columnspan=2, sticky="nsew", padx=6, pady=6)
        self.trend = tk.Canvas(h, bg=CARD, highlightthickness=0)
        self.trend.pack(fill="both", expand=True, padx=6, pady=(0, 6))
        e = Card(body, "Event log")
        e.grid(row=2, column=2, sticky="nsew", padx=6, pady=6)
        self.log = tk.Text(e, bg=CARD, fg=TEXT, bd=0, font=("Consolas", 10), height=6,
                           highlightthickness=0, state="disabled")
        self.log.pack(fill="both", expand=True, padx=12, pady=(0, 8))
        for name, col in [("ok", GREEN), ("warn", AMBER), ("bad", RED), ("dim", MUTED)]:
            self.log.tag_config(name, foreground=col)

        # footer: keypad legend
        foot = tk.Frame(root, bg=BG)
        foot.pack(fill="x", padx=18, pady=(2, 10))
        tk.Label(foot, text="KEYPAD", fg=MUTED, bg=BG, font=(FONT, 9, "bold")).pack(side="left")
        for k, t in [("C", "calibrate (10 flicks = healthy)"), ("B", "mark loose support"),
                     ("D", "clear mark"), ("A", "teach new fault"), ("F", "LCD page")]:
            tk.Label(foot, text=f" {k} ", fg=BG, bg=MUTED, font=(FONT, 9, "bold")).pack(side="left", padx=(14, 4))
            tk.Label(foot, text=t, fg=MUTED, bg=BG, font=(FONT, 9)).pack(side="left")

        for cv in (self.gauge, self.leds, self.wave, self.spec, self.trend):
            cv.bind("<Configure>", lambda ev: self.redraw())
        self.add_event("Dashboard started, listening on " + source, "dim")
        root.after(100, self.poll)
        self.tick()

    # ---------------------------------------------------------------- drawing
    def redraw(self):
        self.draw_gauge()
        self.draw_leds()
        self.draw_wave()
        self.draw_spec()
        self.draw_trend()

    def draw_gauge(self):
        c = self.gauge
        c.delete("all")
        w, h = c.winfo_width(), c.winfo_height()
        r = max(min(w / 2 - 20, h - 30), 40)
        cx, cy = w / 2, h - 18
        box = (cx - r, cy - r, cx + r, cy + r)
        for lo, hi, col in [(0, 40, RED), (40, 70, AMBER), (70, 100, GREEN)]:
            c.create_arc(*box, start=180 - hi * 1.8, extent=(hi - lo) * 1.8, style="arc",
                         outline=col, width=4)
        c.create_arc(cx - r + 14, cy - r + 14, cx + r - 14, cy + r - 14, start=0, extent=180,
                     style="arc", outline=GRID, width=16)
        if self.r:
            hv = self.r["health"]
            c.create_arc(cx - r + 14, cy - r + 14, cx + r - 14, cy + r - 14, start=180,
                         extent=-hv * 1.8, style="arc", outline=health_color(hv), width=16)
            c.create_text(cx, cy - r * 0.38, text=f"{hv}%", fill=health_color(hv),
                          font=(FONT, int(r / 3), "bold"))
            band = "SAFE" if hv >= 70 else "CAUTION" if hv >= 40 else "UNSAFE"
            c.create_text(cx, cy - 8, text=band, fill=MUTED, font=(FONT, 10, "bold"))
        else:
            c.create_text(cx, cy - r * 0.38, text="--", fill=DIM, font=(FONT, int(r / 3), "bold"))
        c.create_text(cx - r + 6, cy + 8, text="0", fill=DIM, font=(FONT, 8))
        c.create_text(cx + r - 6, cy + 8, text="100", fill=DIM, font=(FONT, 8))

    def draw_leds(self):
        c = self.leds
        c.delete("all")
        w = c.winfo_width()
        hv = self.r["health"] if self.r else 0
        n = int(hv / 12.5 + 0.5)
        step = min(44, (w - 20) / 8)
        x0 = w / 2 - step * 4 + step / 2
        for i in range(8):
            x = x0 + i * step
            on = i < n
            col = health_color(hv) if on else GRID
            if on:
                c.create_oval(x - 15, 5, x + 15, 35, fill="", outline=col, width=1)
            c.create_oval(x - 11, 9, x + 11, 31, fill=col, outline=EDGE)

    def axes(self, c, xlab, ylab, xticks, x2px, y_lines=()):
        w, h = c.winfo_width(), c.winfo_height()
        for v, lab in xticks:
            x = x2px(v)
            c.create_line(x, 8, x, h - 22, fill=GRID)
            c.create_text(x, h - 12, text=lab, fill=DIM, font=(FONT, 8))
        for y, lab in y_lines:
            c.create_line(40, y, w - 8, y, fill=GRID)
            c.create_text(34, y, text=lab, fill=DIM, font=(FONT, 8), anchor="e")
        c.create_text(w - 10, 10, text=xlab, fill=MUTED, font=(FONT, 8), anchor="e")
        if ylab:
            c.create_text(44, 10, text=ylab, fill=MUTED, font=(FONT, 8), anchor="w")

    def draw_wave(self):
        c = self.wave
        c.delete("all")
        w, h = c.winfo_width(), c.winfo_height()
        if self.raw is None or w < 60:
            c.create_text(w / 2, h / 2, text="no flick yet", fill=DIM, font=(FONT, 10))
            return
        y = np.asarray(self.raw, float)
        y -= np.median(y)
        span = float(np.abs(y).max()) or 1
        n, L, R, T, B = len(y), 44, w - 10, 12, h - 24
        x2px = lambda t: L + (t * FS) / (n - 1) * (R - L)
        y2px = lambda v: (T + B) / 2 - v / span * (B - T) / 2
        dur = n / FS
        self.axes(c, "time (s)", "", [(t, f"{t:g}") for t in range(0, int(dur) + 1, 2)], x2px,
                  [(y2px(0), "0")])
        idx = np.linspace(0, n - 1, min(n, int(R - L) * 2)).astype(int)
        pts = []
        for i in idx:
            pts += [L + i / (n - 1) * (R - L), y2px(y[i])]
        c.create_line(*pts, fill=BLUE, width=1)
        if self.r:   # decay envelope from the measured damping
            wn, z = 2 * math.pi * self.r["f1"], self.r["damp"]
            peak = float(np.abs(y[:int(FS)]).max())
            for sgn in (1, -1):
                env = []
                for k in range(0, n, 8):
                    env += [L + k / (n - 1) * (R - L), y2px(sgn * peak * math.exp(-z * wn * k / FS))]
                c.create_line(*env, fill=AMBER, width=1, dash=(4, 3))
            c.create_text(R, T + 4, text="- - decay envelope from ζ", fill=AMBER,
                          font=(FONT, 8), anchor="ne")

    def draw_spec(self):
        c = self.spec
        c.delete("all")
        w, h = c.winfo_width(), c.winfo_height()
        if self.raw is None or w < 60:
            c.create_text(w / 2, h / 2, text="no flick yet", fill=DIM, font=(FONT, 10))
            return
        y = np.asarray(self.raw, float)
        y = (y - y.mean()) * np.hanning(len(y))
        mag = np.abs(np.fft.rfft(y))
        fr = np.fft.rfftfreq(len(y), 1 / FS)
        keep = fr <= 10
        fr, mag = fr[keep], mag[keep]
        mag = mag / (mag.max() or 1)
        L, R, T, B = 44, w - 10, 14, h - 24
        x2px = lambda f: L + f / 10 * (R - L)
        y2px = lambda m: B - m * (B - T)
        self.axes(c, "Hz", "", [(f, str(f)) for f in range(0, 11, 2)], x2px)
        pts = [L, B]
        for f, m in zip(fr, mag):
            pts += [x2px(f), y2px(m)]
        pts += [x2px(fr[-1]), B]
        c.create_polygon(*pts, fill="#1f3a5f", outline=BLUE)
        base = self.baseline()[0]
        if base:
            x = x2px(base)
            c.create_line(x, T, x, B, fill=GREEN, dash=(3, 3))
            c.create_text(x - 4, B - 4, text=f"baseline {base:.2f}", fill=GREEN, font=(FONT, 8), anchor="se")
        if self.r:
            x = x2px(self.r["f1"])
            c.create_line(x, T + 14, x, B, fill=TEXT)
            c.create_text(x + 4, T + 14, text=f"f1 {self.r['f1']:.2f} Hz", fill=TEXT,
                          font=(FONT, 8, "bold"), anchor="nw")

    def draw_trend(self):
        c = self.trend
        c.delete("all")
        w, h = c.winfo_width(), c.winfo_height()
        L, R, T, B = 44, w - 10, 28, h - 24
        y2px = lambda v: B - v / 100 * (B - T)
        for lo, hi, col in [(0, 40, "#2a1517"), (40, 70, "#2a2312"), (70, 100, "#132a1a")]:
            c.create_rectangle(L, y2px(hi), R, y2px(lo), fill=col, outline="")
        for v in (0, 40, 70, 100):
            c.create_line(L, y2px(v), R, y2px(v), fill=GRID)
            c.create_text(L - 6, y2px(v), text=str(v), fill=DIM, font=(FONT, 8), anchor="e")
        c.create_text(R, y2px(40) - 2, text="UNSAFE below 40", fill=RED, font=(FONT, 8), anchor="se")
        if not self.hist:
            c.create_text((L + R) / 2, (T + B) / 2, text="no flicks yet", fill=DIM, font=(FONT, 10))
            return
        n = max(len(self.hist), 2)
        x = lambda i: L + 10 + i * (R - L - 20) / (n - 1)
        pts = []
        for i, (num, hv, cls) in enumerate(self.hist):
            pts += [x(i), y2px(hv)]
        if len(pts) >= 4:
            c.create_line(*pts, fill=MUTED, width=2)
        for i, (num, hv, cls) in enumerate(self.hist):
            col = INFO.get(cls, ("", MUTED))[1]
            c.create_oval(x(i) - 5, y2px(hv) - 5, x(i) + 5, y2px(hv) + 5, fill=col, outline=CARD)
            if len(self.hist) <= 20 or i % 2 == 0:
                c.create_text(x(i), B + 12, text=f"#{num}", fill=DIM, font=(FONT, 7))
        lx = L + 6
        for k in ("healthy", "added_mass", "loose_joint", "loose_support"):
            name, col = INFO[k][0], INFO[k][1]
            c.create_oval(lx, 4, lx + 8, 12, fill=col, outline="")
            c.create_text(lx + 12, 8, text=name.title(), fill=MUTED, font=(FONT, 8), anchor="w")
            lx += 14 + len(name) * 7

    # ---------------------------------------------------------------- data
    def baseline(self):
        f = statistics.median(self.healthy_f1[-10:]) if self.healthy_f1 else None
        z = statistics.median(self.healthy_z[-10:]) if self.healthy_z else None
        return f, z

    def add_event(self, text, tag):
        self.log.config(state="normal")
        self.log.insert("1.0", f"{time.strftime('%H:%M:%S')}  {text}\n", tag)
        self.log.delete("200.0", "end")
        self.log.config(state="disabled")

    def poll(self):
        try:
            while True:
                self.show(*self.q.get_nowait())
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def tick(self):
        self.clock.config(text=time.strftime("%d %b %Y   %H:%M:%S"))
        self.root.after(1000, self.tick)

    def show(self, raw, r):
        self.count += 1
        self.raw = raw
        big, delta, sub = self.metric["count"]
        big.config(text=str(self.count))
        if r is None:
            self.cls.config(text="CALIBRATING", fg=MUTED)
            self.desc.config(text="Flick received without a result. The board is taking its 10 healthy "
                                  "baseline flicks (key C).")
            self.add_event(f"#{self.count}  raw flick (calibration)", "dim")
            self.draw_wave()
            self.draw_spec()
            return
        prev = self.r
        self.r = r
        cls, hv = r["knn"], r["health"]
        name, col, text, act = INFO.get(cls, (cls.upper(), MUTED, "", ""))
        if cls == "healthy" and hv >= 95:
            self.healthy_f1.append(r["f1"])
            self.healthy_z.append(r["damp"])
        self.cls.config(text=name, fg=col)
        if r["unsafe"]:
            self.flag.config(text="  UNSAFE  ", bg=RED)
        elif r["unknown"]:
            self.flag.config(text="  UNKNOWN CHANGE  ", bg=AMBER)
        elif r["changed"]:
            self.flag.config(text="  MARKED LOOSE (B)  ", bg=PURPLE)
        else:
            self.flag.config(text="  OK  ", bg=GREEN)
        self.desc.config(text=text)
        self.action.config(text="Suggested: " + act if act else "")
        agree = sum(r[k] == cls for k in ("nn", "tree"))
        for k, lab in self.votes.items():
            v = r[k]
            t = INFO.get(v, (v.upper(),))[0].title()
            if k == "nn":
                t += f"  {r['conf']}%"
            lab.config(text=t, fg=INFO.get(v, ("", MUTED))[1])

        bf, bz = self.baseline()
        big, delta, sub = self.metric["f1"]
        big.config(text=f"{r['f1']:.2f} Hz")
        if bf:
            d = (r["f1"] / bf - 1) * 100
            delta.config(text=f"{d:+.1f} %", fg=GREEN if abs(d) < 3 else AMBER if d > -15 else BLUE)
            sub.config(text=f"baseline {bf:.2f} Hz (median of healthy flicks)")
        big, delta, sub = self.metric["z"]
        big.config(text=f"{r['damp'] * 100:.2f} %")
        if bz:
            d = (r["damp"] - bz) * 100
            delta.config(text=f"{d:+.2f} pts", fg=GREEN if abs(d) < 0.3 else AMBER)
            sub.config(text=f"baseline {bz * 100:.2f} %")
        big, delta, sub = self.metric["count"]
        sub.config(text=f"models agree: {agree + 1}/3 on this flick")

        self.hist = (self.hist + [(self.count, hv, cls)])[-30:]
        tag = "ok" if hv >= 70 else "warn" if hv >= 40 else "bad"
        self.add_event(f"#{self.count:<3} {name:<13} {hv:>3}%  {r['f1']:.2f} Hz", tag)
        if prev and prev["knn"] != cls:
            self.add_event(f"     condition changed: {INFO.get(prev['knn'], (prev['knn'],))[0]} -> {name}", tag)
        self.redraw()


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
