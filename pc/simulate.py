"""Make fake flicks so the whole pipeline can be built before lab week.

A flicked cantilever rings like a decaying sine wave. Each fault changes
that wave in a known way:
  loose clamp  -> lower frequency, much more damping, a little rattle
  loose joint  -> slightly lower frequency, more damping, more rattle
  added mass   -> lower frequency, damping about the same (NOT damage)

Output: data/sim/<class>__<step>__<id>.txt, one ADC count per line,
exactly the format logger.py saves from the real board.

Usage:  python pc/simulate.py            (training set, ~130 flicks)
        python pc/simulate.py --unseen   (faults the model never saw)
"""
import argparse
import math
import numpy as np
from config import FS, N, COUNTS_PER_G, ADC_MAX, ZERO_G, DATA

F0 = 7.0         # healthy two-ruler beam, 35 cm free length (estimate)
ZETA0 = 0.008    # healthy damping ratio

# (class, step, how many flicks)
PLAN = [
    ("healthy", 0, 30),
    ("loose_clamp", 1, 12), ("loose_clamp", 2, 12), ("loose_clamp", 3, 10),
    ("loose_joint", 1, 15), ("loose_joint", 2, 12),
    ("added_mass", 1, 30),
]
UNSEEN = [("unseen_joint_and_coin", 1, 10), ("unseen_clamp_and_coin", 1, 10)]


def params(cls, step, rng):
    """Physical parameters (freq, damping, rattle) for one flick."""
    f, z, rattle = F0, ZETA0, 0.0
    if cls == "loose_clamp":
        s = step / 4
        f *= 1 - 0.04 * s
        z += 0.030 * s
        rattle = 0.03 * s
    elif cls == "loose_joint":
        s = step / 2
        f *= 1 - 0.06 * s
        z += 0.006 * s
        rattle = 0.20 * s
    elif cls == "added_mass":
        f *= 0.93
        z *= 0.95
    elif cls == "unseen_joint_and_coin":
        f *= 0.93 * (1 - 0.03)
        z += 0.003
        rattle = 0.10
    elif cls == "unseen_clamp_and_coin":
        f *= 0.93 * (1 - 0.035)
        z += 0.015
        rattle = 0.02
    # flick-to-flick and re-clamping variation
    f *= 1 + rng.normal(0, 0.004)
    z *= 1 + rng.normal(0, 0.08)
    return f, max(z, 0.002), rattle


def flick(cls, step, rng):
    f, z, rattle = params(cls, step, rng)
    t = np.arange(N) / FS
    w = 2 * math.pi * f
    env = np.exp(-z * w * t)
    amp = rng.uniform(0.6, 2.4)                       # how hard you flicked, in g
    x = amp * env * np.sin(w * np.sqrt(1 - z * z) * t)
    # rattle: a loose part knocks twice per cycle -> 2nd harmonic + clicks
    x += rattle * amp * env * np.sin(2 * w * t + rng.uniform(0, 6.28))
    x += rattle * amp * env * rng.normal(0, 0.3, N)
    # 2nd bending mode (about 6.27 x f1), mostly filtered out by the sensor
    x += 0.05 * amp * np.exp(-3 * z * w * t) * np.sin(6.27 * w * t)
    # sensor noise and a small tilt offset
    x += rng.normal(0, 0.01, N) + rng.normal(0, 0.02)
    counts = np.round(ZERO_G + x * COUNTS_PER_G)
    return np.clip(counts, 0, ADC_MAX).astype(int)    # hard flicks clip at the rails


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unseen", action="store_true")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed + (100 if args.unseen else 0))
    out = DATA / ("sim_unseen" if args.unseen else "sim")
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*.txt"):
        old.unlink()
    total = 0
    for cls, step, count in (UNSEEN if args.unseen else PLAN):
        for i in range(count):
            np.savetxt(out / f"{cls}__{step}__{i:03d}.txt", flick(cls, step, rng), fmt="%d")
            total += 1
    print(f"wrote {total} flicks to {out}")


if __name__ == "__main__":
    main()
