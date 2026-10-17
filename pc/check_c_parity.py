"""Check that the board C code gives the same answers as the Python code.

Compiles board/*.c with gcc on the laptop, runs it on every flick file,
and compares features and predictions with Python.
Usage:  python pc/check_c_parity.py sim
"""
import io
import pickle
import subprocess
import sys
import tempfile
from pathlib import Path
import numpy as np
import pandas as pd
from sklearn.neighbors import KNeighborsClassifier, KNeighborsRegressor
from config import ROOT, DATA, RESULTS, CLASSES, FEATURES
from features import extract, load_raw

name = sys.argv[1] if len(sys.argv) > 1 else "sim"
exe = Path(tempfile.gettempdir()) / ("host_test.exe" if sys.platform == "win32" else "host_test")
b = ROOT / "board"
subprocess.run(["gcc", "-O2", "-Wall", str(b / "host_test.c"), str(b / "features.c"),
                str(b / "classify.c"), "-lm", "-o", str(exe)], check=True)
files = sorted((DATA / name).glob("*.txt"))
c = pd.read_csv(io.StringIO(subprocess.run([str(exe)] + [str(f) for f in files],
                                           capture_output=True, text=True, check=True).stdout))

m = pickle.load(open(RESULTS / name / "models.pkl", "rb"))
P = np.array([extract(load_raw(f)) for f in files])
Ps = m["scaler"].transform(P)
knn = KNeighborsClassifier(3).fit(m["Xs"], m["y"])
lab = m["damage"] >= 0
reg = KNeighborsRegressor(3).fit(m["Xs"][lab], m["damage"][lab])
py = {"tree": m["tree"].predict(P), "nn": m["mlp"].predict(Ps), "knn": knn.predict(Ps)}
health = np.clip(100 * (1 - reg.predict(Ps)), 0, 100)

ok = True
for i, f in enumerate(FEATURES):
    rel = np.max(np.abs(c[f] - P[:, i]) / (np.abs(P[:, i]) + 1e-6))
    print(f"  {f:13s} max relative difference {rel:.2e}")
    absdiff = np.max(np.abs(c[f] - P[:, i]))
    # the board uses 32-bit floats, so tiny values can differ by ~1e-6 in absolute terms
    ok &= rel < 1e-3 or absdiff < 1e-5 or f == "decay_s" and absdiff <= 0.25
names = np.array([s.upper() for s in CLASSES])
for k in py:
    agree = (c[k].to_numpy() == names[py[k]]).mean()
    print(f"  {k:13s} predictions agree {agree * 100:.1f} %")
    ok &= agree > 0.97
hd = np.max(np.abs(c.health - health))
print(f"  health        max difference {hd:.2f} points")
ok &= hd < 1.0
print("PARITY OK" if ok else "PARITY PROBLEM: C and Python disagree")
sys.exit(0 if ok else 1)
