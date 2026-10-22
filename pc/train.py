"""Train every model, make the report plots, and write board/model.h.

Usage:  python pc/train.py sim     (fake data, for practice)
        python pc/train.py real    (after recording in the lab)

What it trains:
  1. Frequency-only tree   (the "no ML" baseline: just looks at f1)
  2. Decision tree         (depth 3, all 5 features)
  3. kNN, k = 3            (also used for teach-on-the-spot on the board)
  4. Tiny neural network   (5 -> 8 -> 4, ReLU)
  5. kNN regression        (damage level -> 0..100 health score),
                           compared against plain linear regression
  6. Change detector       (z-score distance from the healthy baseline)
  7. Novelty detector      (distance to nearest known flick -> "UNKNOWN")
"""
import json
import pickle
import sys
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler
from sklearn.tree import DecisionTreeClassifier
from sklearn.neighbors import KNeighborsClassifier, KNeighborsRegressor
from sklearn.neural_network import MLPClassifier
from sklearn.linear_model import Ridge
from sklearn.pipeline import make_pipeline
from sklearn.model_selection import RepeatedStratifiedKFold, cross_val_predict, cross_val_score, StratifiedKFold, KFold
from sklearn.metrics import confusion_matrix, mean_absolute_error
from config import DATA, RESULTS, CLASSES, FEATURES, UNSAFE_HEALTH
from export_c import write_model_h

# Categorical palette, fixed order (one colour + marker per class)
COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
MARKERS = ["o", "s", "^", "D"]
INK, INK2, GRID = "#1f1f1e", "#5f5e58", "#e4e3dd"
NICE = {"healthy": "Healthy", "loose_support": "Loose support",
        "loose_joint": "Loose joint", "added_mass": "Added mass (coin)"}


def style(ax):
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK2, labelsize=9)
    ax.grid(color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)


def models():
    return {
        "Frequency only (no ML)": DecisionTreeClassifier(max_depth=2, random_state=0),
        "Decision tree": DecisionTreeClassifier(max_depth=3, random_state=0),
        "kNN (k=3)": make_pipeline(StandardScaler(), KNeighborsClassifier(3)),
        "Neural net (5-8-4)": make_pipeline(
            StandardScaler(),
            MLPClassifier((8,), max_iter=4000, alpha=1e-3, random_state=0)),
    }


def main(name):
    df = pd.read_csv(DATA / f"{name}_features.csv")
    df = df[df.label.isin(CLASSES)].reset_index(drop=True)
    X = df[FEATURES].to_numpy()
    y = df.label.map({c: i for i, c in enumerate(CLASSES)}).to_numpy()
    out = RESULTS / name
    out.mkdir(parents=True, exist_ok=True)
    print(f"{len(df)} flicks:", df.label.value_counts().to_dict())

    # ---- classifier comparison (cross-validated, so scores are honest) ----
    n_splits = max(2, min(5, int(np.bincount(y).min())))
    cv = RepeatedStratifiedKFold(n_splits=n_splits, n_repeats=5, random_state=0)
    acc, cms = {}, {}
    for label, m in models().items():
        Xm = X[:, :1] if label.startswith("Frequency") else X
        acc[label] = float(cross_val_score(m, Xm, y, cv=cv).mean())
        pred = cross_val_predict(m, Xm, y, cv=StratifiedKFold(n_splits, shuffle=True, random_state=0))
        cms[label] = confusion_matrix(y, pred, labels=range(len(CLASSES)))
        print(f"  {label:26s} accuracy {acc[label]*100:5.1f} %")

    # ---- final models on all data (these go to the board) ----
    scaler = StandardScaler().fit(X)
    Xs = scaler.transform(X)
    tree = DecisionTreeClassifier(max_depth=3, random_state=0).fit(X, y)
    mlp = MLPClassifier((8,), max_iter=4000, alpha=1e-3, random_state=0).fit(Xs, y)

    # health score regression (only rows with a known damage level).
    # A straight-line fit cannot handle "coin lowers f1 but is not damage",
    # so kNN regression (average damage of the 3 most similar flicks) is used
    # on the board; linear regression is kept as the comparison.
    dm = df.damage.notna().to_numpy()
    damage = df.damage.to_numpy()
    kf = KFold(5, shuffle=True, random_state=0)
    health_true = 100 * (1 - damage[dm])
    health_mae = {}
    for label, reg in [("Linear regression", Ridge(alpha=1.0)),
                       ("kNN regression (k=3)", KNeighborsRegressor(3))]:
        p = cross_val_predict(reg, Xs[dm], damage[dm], cv=kf)
        hp = np.clip(100 * (1 - p), 0, 100)
        health_mae[label] = float(mean_absolute_error(health_true, hp))
        print(f"  Health error, {label:21s} {health_mae[label]:5.1f} points")
    health_cv = hp  # kNN version, for the plot
    knn_damage = np.where(dm, damage, -1.0)  # -1 = flick has no damage label

    # change detector: distance from the healthy baseline
    h = X[y == 0]
    h_mean, h_std = h.mean(0), h.std(0) + 1e-9
    zdist = lambda A: np.sqrt((((A - h_mean) / h_std) ** 2).mean(1))
    change_thr = float(max(3.0, 1.5 * zdist(h).max()))

    # novelty detector: distance to the nearest stored flick (scaled space)
    d = np.linalg.norm(Xs[:, None] - Xs[None], axis=2)
    np.fill_diagonal(d, np.inf)
    nn = d.min(1)
    novelty_thr = float(2.0 * np.percentile(nn, 95))

    # try the detectors on faults the model never saw, if that set exists
    unseen = {}
    up = DATA / f"{name}_unseen_features.csv"
    if up.exists():
        U = pd.read_csv(up)[FEATURES].to_numpy()
        Us = scaler.transform(U)
        nn_u = np.linalg.norm(Us[:, None] - Xs[None], axis=2).min(1)
        unseen = {"flicks": len(U),
                  "flagged_unknown": int((nn_u > novelty_thr).sum()),
                  "flagged_changed": int((zdist(U) > change_thr).sum())}
        print(f"  Unseen faults flagged UNKNOWN: {unseen['flagged_unknown']}/{len(U)}")

    # ---- plots ----
    plot_scatter(df, out)
    plot_accuracy(acc, out)
    plot_confusions(cms, out)
    plot_health(health_true, health_cv, df.label[dm].to_numpy(), out)

    metrics = {"dataset": name, "flicks": len(df), "accuracy": acc,
               "health_mae_points": health_mae, "change_threshold": change_thr,
               "novelty_threshold": novelty_thr, "unseen_test": unseen,
               "tree_feature_importance": dict(zip(FEATURES, tree.feature_importances_.round(3).tolist())),
               "healthy_baseline_mean": dict(zip(FEATURES, h_mean.round(5).tolist()))}
    (out / "metrics.json").write_text(json.dumps(metrics, indent=2))

    with open(out / "models.pkl", "wb") as fh:
        pickle.dump({"scaler": scaler, "tree": tree, "mlp": mlp, "Xs": Xs, "y": y,
                     "damage": knn_damage}, fh)
    write_model_h(scaler, tree, mlp, knn_damage, Xs, y, h_mean, h_std,
                  change_thr, novelty_thr, UNSAFE_HEALTH, name)
    print(f"plots + metrics.json in {out}")


def plot_scatter(df, out):
    fig, ax = plt.subplots(figsize=(6.4, 4.4), dpi=150)
    for i, c in enumerate(CLASSES):
        s = df[df.label == c]
        ax.scatter(s.f1_hz, s.damping * 100, s=26, c=COLORS[i], marker=MARKERS[i],
                   edgecolors="white", linewidths=0.8, label=NICE[c], zorder=3)
    style(ax)
    ax.set_xlabel("Natural frequency f1 (Hz)", color=INK)
    ax.set_ylabel("Damping ratio (%)", color=INK)
    ax.set_title("Frequency alone mixes up the coin and real damage;\ndamping separates them",
                 color=INK, fontsize=11, loc="left")
    ax.legend(frameon=False, fontsize=9, labelcolor=INK)
    fig.tight_layout()
    fig.savefig(out / "features_f1_vs_damping.png")
    plt.close(fig)


def plot_accuracy(acc, out):
    fig, ax = plt.subplots(figsize=(6.4, 3.0), dpi=150)
    names = list(acc)[::-1]
    vals = [acc[n] * 100 for n in names]
    bars = ax.barh(names, vals, color="#2a78d6", height=0.55)
    for b, v in zip(bars, vals):
        ax.text(v + 1, b.get_y() + b.get_height() / 2, f"{v:.1f} %", va="center", color=INK, fontsize=9)
    style(ax)
    ax.grid(axis="y", visible=False)
    ax.set_xlim(0, 110)
    ax.set_xlabel("Cross-validated accuracy (%)", color=INK)
    ax.set_title("Classifier accuracy", color=INK, fontsize=11, loc="left")
    fig.tight_layout()
    fig.savefig(out / "accuracy_comparison.png")
    plt.close(fig)


def plot_confusions(cms, out):
    keys = ["Frequency only (no ML)", "Decision tree", "Neural net (5-8-4)"]
    fig, axes = plt.subplots(1, 3, figsize=(12, 3.9), dpi=150)
    short = ["Healthy", "Support", "Joint", "Coin"]
    for ax, k in zip(axes, keys):
        cm = cms[k]
        ax.imshow(cm, cmap="Blues")
        for i in range(cm.shape[0]):
            for j in range(cm.shape[1]):
                ax.text(j, i, cm[i, j], ha="center", va="center", fontsize=10,
                        color="white" if cm[i, j] > cm.max() / 2 else INK)
        ax.set_xticks(range(4), short, fontsize=9)
        ax.set_yticks(range(4), short, fontsize=9)
        ax.set_xlabel("Predicted", color=INK2)
        ax.set_ylabel("Actual", color=INK2)
        ax.set_title(k, color=INK, fontsize=10)
    fig.tight_layout()
    fig.savefig(out / "confusion_matrices.png")
    plt.close(fig)


def plot_health(true, pred, labels, out):
    fig, ax = plt.subplots(figsize=(5.2, 4.4), dpi=150)
    for i, c in enumerate(CLASSES):
        m = labels == c
        ax.scatter(true[m] + np.random.default_rng(i).normal(0, 1.2, m.sum()), pred[m], s=22,
                   c=COLORS[i], marker=MARKERS[i], edgecolors="white", linewidths=0.8, label=NICE[c], zorder=3)
    ax.plot([0, 100], [0, 100], color=INK2, linewidth=1, linestyle="--", zorder=2)
    ax.axhline(UNSAFE_HEALTH, color="#d03b3b", linewidth=1, zorder=2)
    ax.text(2, UNSAFE_HEALTH + 2, "Unsafe line", color=INK2, fontsize=8)
    style(ax)
    ax.set_xlabel("True health (%)", color=INK)
    ax.set_ylabel("Predicted health (%)", color=INK)
    ax.set_title("Health score from kNN regression", color=INK, fontsize=11, loc="left")
    ax.legend(frameon=False, fontsize=8, labelcolor=INK)
    fig.tight_layout()
    fig.savefig(out / "health_score.png")
    plt.close(fig)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "sim")
