"""Re-run the latest trained models over every flick in data/<set>_log.csv.

The logger writes the answer of whatever model was on the laptop at
recording time (on the first lab day that was the sim-trained one). After
python pc/train.py real, run this so the log, trend.py and llm_report.py
use the real-data models.

Usage:  python pc/rescore_log.py real
"""
import sys
import pandas as pd
from config import DATA
from features import load_raw
from predict import Predictor

name = sys.argv[1] if len(sys.argv) > 1 else "real"
log_path = DATA / f"{name}_log.csv"
log = pd.read_csv(log_path, dtype=str, keep_default_na=False)
pred = Predictor(name)
for i, f in enumerate(log.file):
    for k, v in pred(load_raw(DATA / name / f)).items():
        if k in log.columns:
            log.at[i, k] = str(v)
log.to_csv(log_path, index=False)
print(f"rescored {len(log)} flicks in {log_path}")
