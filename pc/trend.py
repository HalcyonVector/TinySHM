"""Predict when the structure becomes unsafe (a mini remaining-life estimate).

Takes the health scores logged while you loosen the clamp step by step
(healthy = step 0, then 1/4 turn, 1/2 turn ...), fits a straight line
(linear regression) through them and extrapolates to the UNSAFE line.

Usage:  python pc/trend.py real --fault loose_clamp --upto 2
        (use only steps 0..2 to predict, then check against step 3 if you recorded it)
Writes results/<set>/trend.png and trend.json
"""
import argparse
import json
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from config import DATA, RESULTS, UNSAFE_HEALTH

INK, INK2, GRID, BLUE = "#1f1f1e", "#5f5e58", "#e4e3dd", "#2a78d6"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("set", nargs="?", default="real", help="real or sim_session")
    ap.add_argument("--fault", default="loose_clamp")
    ap.add_argument("--upto", type=int, default=None, help="only use steps up to this to fit")
    args = ap.parse_args()

    log = pd.read_csv(DATA / f"{args.set}_log.csv")
    d = log[(log.label == "healthy") | (log.label == args.fault)].copy()
    d.loc[d.label == "healthy", "step"] = 0
    per_step = d.groupby("step").health.agg(["mean", "std", "count"]).reset_index()
    fit = per_step if args.upto is None else per_step[per_step.step <= args.upto]
    if len(fit) < 2:
        raise SystemExit("need at least 2 steps (healthy + one fault step) to fit a trend")

    slope, intercept = np.polyfit(fit.step, fit["mean"], 1)
    cross = (UNSAFE_HEALTH - intercept) / slope if slope < 0 else float("inf")
    out = {"fault": args.fault, "fitted_steps": fit.step.tolist(),
           "health_per_step": dict(zip(per_step.step.astype(int).astype(str), per_step["mean"].round(1))),
           "slope_per_step": round(float(slope), 2), "intercept": round(float(intercept), 1),
           "predicted_unsafe_step": round(float(cross), 2)}
    print(f"health drops {-slope:.1f} points per step; predicted UNSAFE at step {cross:.2f}")

    res = RESULTS / args.set
    res.mkdir(parents=True, exist_ok=True)
    (res / "trend.json").write_text(json.dumps(out, indent=2))

    fig, ax = plt.subplots(figsize=(6.0, 4.0), dpi=150)
    ax.scatter(d.step + np.random.default_rng(0).normal(0, 0.04, len(d)), d.health,
               s=14, c=BLUE, alpha=0.35, edgecolors="none", zorder=2, label="Each flick")
    ax.scatter(per_step.step, per_step["mean"], s=46, c=BLUE, edgecolors="white",
               linewidths=1.2, zorder=4, label="Average per step")
    xmax = max(per_step.step.max(), cross if np.isfinite(cross) else 0) + 0.5
    xs = np.linspace(0, xmax, 50)
    ax.plot(xs, intercept + slope * xs, color=INK2, linestyle="--", linewidth=1.2, zorder=3,
            label="Linear regression trend")
    ax.axhline(UNSAFE_HEALTH, color="#d03b3b", linewidth=1)
    ax.text(0.02, UNSAFE_HEALTH - 6, "Unsafe line", color=INK2, fontsize=8)
    if np.isfinite(cross):
        ax.axvline(cross, color=INK2, linewidth=0.8, linestyle=":")
        ax.text(cross + 0.05, 92, f"predicted\nunsafe at\nstep {cross:.1f}", color=INK, fontsize=8)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.grid(color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    ax.set_ylim(0, 105)
    ax.set_xlabel(f"Damage step ({args.fault.replace('_', ' ')}: quarter turns)", color=INK)
    ax.set_ylabel("Health score (%)", color=INK)
    ax.set_title("Predicting when the structure becomes unsafe", color=INK, fontsize=11, loc="left")
    ax.legend(frameon=False, fontsize=8, loc="lower left")
    fig.tight_layout()
    fig.savefig(res / "trend.png")
    print(f"wrote {res / 'trend.png'}")


if __name__ == "__main__":
    main()
