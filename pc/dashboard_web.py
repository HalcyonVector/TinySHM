"""Browser dashboard for the demo (same data as dashboard.py, nicer rendering).

Reads the board's RES,... and RAW,... lines over USB serial, and serves a
page on http://localhost:8765 that updates live for every flick. Works
offline: the page is in pc/dashboard.html and uses no internet.

  py pc/dashboard_web.py --port COM4
Practice without a board:
  py pc/dashboard_web.py --demo
"""
import argparse
import json
import math
import random
import queue
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import numpy as np

from config import FS
from logger import open_serial, parse_lines


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


PAGE = Path(__file__).with_name("dashboard.html")
clients, lock = [], threading.Lock()


def packet(raw, r, n, source):
    y = np.asarray(raw, float)
    y -= np.median(y)
    spec = np.abs(np.fft.rfft((y - y.mean()) * np.hanning(len(y))))
    fr = np.fft.rfftfreq(len(y), 1 / FS)
    keep = fr <= 10
    spec = spec[keep] / (spec[keep].max() or 1)
    step = max(1, len(y) // 600)
    return {"n": n, "t": time.strftime("%H:%M:%S"), "source": source, "res": r,
            "wave": [round(v, 1) for v in y[::step]], "dur": len(y) / FS,
            "spec": [round(float(v), 4) for v in spec], "fmax": float(fr[keep][-1])}


def pump(q, source):
    n = 0
    while True:
        raw, r = q.get()
        n += 1
        msg = ("data: " + json.dumps(packet(raw, r, n, source)) + "\n\n").encode()
        with lock:
            for c in clients:
                c.put(msg)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path.startswith("/events"):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            mine = queue.Queue()
            with lock:
                clients.append(mine)
            try:
                self.wfile.write(b": hello\n\n")
                self.wfile.flush()
                while True:
                    try:
                        self.wfile.write(mine.get(timeout=15))
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                    self.wfile.flush()
            except OSError:
                pass
            finally:
                with lock:
                    clients.remove(mine)
            return
        body = PAGE.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(body)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="e.g. COM4")
    ap.add_argument("--baud", type=int, default=19200)
    ap.add_argument("--demo", action="store_true", help="fake flicks, no board needed")
    ap.add_argument("--http", type=int, default=8765)
    a = ap.parse_args()
    if not a.demo and not a.port:
        ap.error("give --port COMx, or --demo")
    q = queue.Queue()
    source = "DEMO" if a.demo else a.port
    target, args = (demo_reader, (q,)) if a.demo else (board_reader, (a.port, a.baud, q))
    threading.Thread(target=target, args=args, daemon=True).start()
    threading.Thread(target=pump, args=(q, source), daemon=True).start()
    url = f"http://localhost:{a.http}/"
    print(f"Dashboard at {url}  (Ctrl+C to stop)")
    threading.Timer(1.0, webbrowser.open, args=(url,)).start()
    ThreadingHTTPServer(("127.0.0.1", a.http), Handler).serve_forever()


if __name__ == "__main__":
    main()
