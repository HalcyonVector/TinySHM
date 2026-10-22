"""Compare the 3 frequency methods (synopsis objective 5).

Usage:  python pc/compare_methods.py sim     (or real, after the lab)

Accuracy: each method is compared with a fine PC reference (16x zero-padded
FFT, 0.006 Hz bins) on every recorded flick.
Speed: if the board sent BENCH lines while you logged (data/<set>_log.csv,
column board_bench), their SysTick timings are averaged too.
Writes results/<set>/method_comparison.png and method_comparison.json.
"""
import json
import sys
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from config import DATA, RESULTS
from features import load_raw
import freq_methods as fm

METHODS = [("FFT peak bin", lambda a: fm.fft_peak(a, False)),
           ("Interpolated FFT", lambda a: fm.fft_peak(a, True)),
           ("Zero-crossing", fm.zero_crossing)]
# RAM each method needs on the board beyond the 1024 samples (2 KB)
RAM_BYTES = {"FFT peak bin": 8192, "Interpolated FFT": 8192, "Zero-crossing": 0}


def main(name):
    files = sorted((DATA / name).glob("*.txt"))
    if not files:
        sys.exit(f"no flicks in {DATA / name}")
    ref = np.array([fm.reference(load_raw(f)) for f in files])
    out = {"dataset": name, "flicks": len(files), "methods": {}}
    errs = {}
    for label, fn in METHODS:
        est = np.array([fn(load_raw(f)) for f in files])
        e = est - ref
        errs[label] = np.abs(e)
        out["methods"][label] = {"mean_abs_error_hz": float(np.mean(np.abs(e))),
                                 "max_abs_error_hz": float(np.max(np.abs(e))),
                                 "mean_abs_error_pct": float(np.mean(np.abs(e) / ref) * 100),
                                 "ram_bytes": RAM_BYTES[label]}

    log = DATA / f"{name}_log.csv"
    if log.exists():
        df = pd.read_csv(log)
        if "board_bench" in df and df.board_bench.notna().any():
            b = np.array([[float(v) for v in s.split(",")] for s in df.board_bench.dropna()])
            for i, (label, _) in enumerate(METHODS):
                out["methods"][label]["board_time_us"] = float(b[:, 3 + i].mean())

    print(f"{len(files)} flicks, reference = 16x zero-padded FFT")
    print(f"  {'method':18s} {'mean error':>11s} {'max error':>10s} {'RAM':>7s} {'board time':>11s}")
    for label, m in out["methods"].items():
        t = f"{m['board_time_us'] / 1000:.1f} ms" if "board_time_us" in m else "(board)"
        print(f"  {label:18s} {m['mean_abs_error_hz']:8.3f} Hz {m['max_abs_error_hz']:7.3f} Hz "
              f"{m['ram_bytes'] // 1024:4d} KB {t:>11s}")

    res = RESULTS / name
    res.mkdir(parents=True, exist_ok=True)
    (res / "method_comparison.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    fig, ax = plt.subplots(figsize=(6.4, 3.0), dpi=150)
    labels = [l for l, _ in METHODS][::-1]
    vals = [out["methods"][l]["mean_abs_error_hz"] * 1000 for l in labels]
    bars = ax.barh(labels, vals, color="#2a78d6", height=0.55)
    for bar, v in zip(bars, vals):
        ax.text(v * 1.02 + 0.5, bar.get_y() + bar.get_height() / 2, f"{v:.0f} mHz",
                va="center", color="#1f1f1e", fontsize=9)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color("#e4e3dd")
    ax.tick_params(colors="#5f5e58", labelsize=9)
    ax.grid(axis="x", color="#e4e3dd", linewidth=0.6)
    ax.set_axisbelow(True)
    ax.set_xlim(0, max(vals) * 1.3)
    ax.set_xlabel("Mean frequency error vs reference (mHz)", color="#1f1f1e")
    ax.set_title("Frequency estimation methods", color="#1f1f1e", fontsize=11, loc="left")
    fig.tight_layout()
    fig.savefig(res / "method_comparison.png")
    print(f"wrote {res / 'method_comparison.png'}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "sim")
