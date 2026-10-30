"""Raw flick files -> one features table (CSV).

Usage:  python pc/build_dataset.py sim      (fake data)
        python pc/build_dataset.py real     (your lab recordings)
Writes data/<set>_features.csv
"""
import csv
import sys
from config import DATA, FEATURES, DAMAGE_LEVEL
from features import extract, load_raw, normalize
import statistics


def session_days():
    """File name -> lab day, from the time column of the readings log."""
    with open(DATA / "real_log.csv", encoding="utf-8") as fh:
        return {r["file"].replace("\\", "/").split("/")[-1]: r["time"][:10]
                for r in csv.DictReader(fh)}


def session_baselines(files, days):
    """Per lab day: median healthy frequency and damping, as the board's
    calibration takes them from its 10 flicks."""
    raw = {}
    for p in files:
        if p.stem.split("__")[0] == "healthy":
            raw.setdefault(days.get(p.name, "?"), []).append(extract(load_raw(p)))
    return {d: tuple(statistics.median(f[i] for f in v) for i in range(5))
            for d, v in raw.items()}


def build(name):
    src = DATA / name
    files = sorted(src.glob("*.txt"))
    if not files:
        sys.exit(f"no flick files in {src}")
    out = DATA / f"{name}_features.csv"
    days, bases = {}, {}
    if name == "real":
        days = session_days()
        bases = session_baselines(files, days)
    with open(out, "w", newline="", encoding="utf-8") as fh:
        wr = csv.writer(fh)
        wr.writerow(["file", "label", "step", "damage"] + FEATURES)
        for p in files:
            label, step, _ = p.stem.split("__")
            damage = DAMAGE_LEVEL.get(f"{label}:{step}", "")
            feats = extract(load_raw(p))
            if name == "real":
                feats = normalize(feats, *bases[days.get(p.name, "?")])
            wr.writerow([p.name, label, step, damage] + [f"{v:.6g}" for v in feats[:len(FEATURES)]])
    print(f"{len(files)} flicks -> {out}")
    return out


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "sim")
