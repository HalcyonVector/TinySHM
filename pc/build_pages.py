"""Builds the static GitHub Pages demo: the live dashboard replaying real
recorded flicks (no board needed). Writes pages/index.html.
  py pc/build_pages.py
"""
import json
import re
from pathlib import Path

from config import DATA, ROOT
from dashboard_web import PAGE, packet
from features import extract, load_raw
from predict import Predictor

# day-1 recordings (file numbers below 100), in the order a demo would show them
SCENE = [("healthy__0", 10, 20), ("added_mass__1", 0, 3), ("healthy__0", 20, 22),
         ("loose_joint__1", 0, 3), ("loose_joint__2", 0, 3), ("healthy__0", 22, 25)]


def files(prefix, a, b):
    return sorted(p for p in (DATA / "real").glob(f"{prefix}__*.txt") if a <= int(p.stem[-3:]) < b)


def res(r):
    return {"nn": r["nn"], "conf": round(100 * r["nn_conf"]), "tree": r["tree"], "knn": r["knn"],
            "health": round(r["health"]), "f1": r["f1_hz"], "damp": r["damping"],
            "changed": False, "unknown": bool(r["unknown"]), "unsafe": bool(r["unsafe"])}


def main():
    pred = Predictor("real")
    calib = files("healthy__0", 0, 10)
    feats = [extract(load_raw(p)) for p in calib]
    pred.base = tuple(sorted(f[i] for f in feats)[len(feats) // 2] for i in range(5))
    out, n = [], 0
    for p in calib:
        n += 1
        out.append(packet(load_raw(p).tolist(), None, n, "DEMO"))
    for prefix, a, b in SCENE[1:]:
        for p in files(prefix, a, b):
            n += 1
            raw = load_raw(p)
            out.append(packet(raw.tolist(), res(pred(raw)), n, "DEMO"))
    html = PAGE.read_text(encoding="utf-8")
    replay = ("const FLICKS=" + json.dumps(out, separators=(",", ":")) + ";\n"
              "let k=0;$(\"src\").textContent=\"DEMO · recorded flicks\";$(\"pulse\").style.background=C.info;\n"
              "function next(){const p=Object.assign({},FLICKS[k%FLICKS.length]);p.n=k+1;"
              "p.t=new Date().toLocaleTimeString().slice(0,8);show(p);k++;setTimeout(next,k%FLICKS.length?2600:6000)}\n"
              "setTimeout(next,800);\n")
    html, cnt = re.subn(r"const es=new EventSource.*?show\(p\)\};\n", lambda _: replay, html, flags=re.S)
    assert cnt == 1, "dashboard.html changed: EventSource block not found"
    html = html.replace("Dashboard online, waiting for the board",
                        "Demo: replaying real flicks recorded in the lab (calibration, coin, loose joints)")
    dst = ROOT / "pages" / "index.html"
    dst.parent.mkdir(exist_ok=True)
    dst.write_text(html, encoding="utf-8")
    print(f"{len(out)} flicks -> {dst}")


if __name__ == "__main__":
    main()
