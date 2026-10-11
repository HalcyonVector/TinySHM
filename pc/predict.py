"""Python twin of the board's classify.c: raw flick -> result dict.
Uses the models saved by train.py (results/<set>/models.pkl)."""
import pickle
import numpy as np
from config import RESULTS, CLASSES, FEATURES, UNSAFE_HEALTH
from features import extract
import json


class Predictor:
    def __init__(self, model_set):
        m = pickle.load(open(RESULTS / model_set / "models.pkl", "rb"))
        met = json.loads((RESULTS / model_set / "metrics.json").read_text())
        self.m = m
        self.lab = m["damage"] >= 0
        self.h_mean = np.array([met["healthy_baseline_mean"][f] for f in FEATURES])
        X = m["scaler"].inverse_transform(m["Xs"])
        self.h_std = X[m["y"] == 0].std(0) + 1e-9
        self.change_thr = met["change_threshold"]
        self.novelty_thr = met["novelty_threshold"]

    def __call__(self, adc):
        m = self.m
        f = np.array(extract(adc))
        xs = m["scaler"].transform(f[None])[0]
        d = np.linalg.norm(m["Xs"] - xs, axis=1)
        near = np.argsort(d, kind="stable")[:3]
        knn = np.bincount(m["y"][near], minlength=len(CLASSES))
        dl = np.where(self.lab, d, np.inf)
        health = float(np.clip(100 * (1 - m["damage"][np.argsort(dl, kind="stable")[:3]].mean()), 0, 100))
        change = float(np.sqrt((((f - self.h_mean) / self.h_std) ** 2).mean()))
        proba = m["mlp"].predict_proba(xs[None])[0]
        return {
            **dict(zip(FEATURES, f.round(5).tolist())),
            "tree": CLASSES[int(m["tree"].predict(f[None])[0])],
            "nn": CLASSES[int(proba.argmax())], "nn_conf": round(float(proba.max()), 3),
            "knn": CLASSES[int(knn.argmax())],
            "health": round(health, 1), "change": round(change, 3), "novelty": round(float(d.min()), 3),
            "changed": int(change > self.change_thr), "unknown": int(d.min() > self.novelty_thr),
            "unsafe": int(health < UNSAFE_HEALTH),
        }
