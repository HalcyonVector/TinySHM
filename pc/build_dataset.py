"""Raw flick files -> one features table (CSV).

Usage:  python pc/build_dataset.py sim      (fake data)
        python pc/build_dataset.py real     (your lab recordings)
Writes data/<set>_features.csv
"""
import csv
import sys
from config import DATA, FEATURES, DAMAGE_LEVEL
from features import extract, load_raw


def build(name):
    src = DATA / name
    files = sorted(src.glob("*.txt"))
    if not files:
        sys.exit(f"no flick files in {src}")
    out = DATA / f"{name}_features.csv"
    with open(out, "w", newline="", encoding="utf-8") as fh:
        wr = csv.writer(fh)
        wr.writerow(["file", "label", "step", "damage"] + FEATURES)
        for p in files:
            label, step, _ = p.stem.split("__")
            damage = DAMAGE_LEVEL.get(f"{label}:{step}", "")
            feats = extract(load_raw(p))
            wr.writerow([p.name, label, step, damage] + [f"{v:.6g}" for v in feats])
    print(f"{len(files)} flicks -> {out}")
    return out


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "sim")
