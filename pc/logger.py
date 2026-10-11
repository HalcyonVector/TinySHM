"""Record flicks from the board in the lab.

The board sends one line per flick:  RAW,<1024 ADC values>
(and, once the model is on it, a RES,... line with its own answer).

Usage (in the lab):
  python pc/logger.py --port COM5 --label healthy --step 0 --count 25
  python pc/logger.py --port COM5 --label loose_clamp --step 1 --count 12
Practice without a board:
  python pc/logger.py --sim --label loose_clamp --step 2 --count 3

Saves every flick to data/<set>/<label>__<step>__<id>.txt (set = real, or
sim_session with --sim) and appends one row per flick to
data/<set>_log.csv, which trend.py and llm_report.py read.
"""
import argparse
import csv
import time
from datetime import datetime
import numpy as np
from config import DATA, N, CLASSES, FEATURES

LABELS = CLASSES + ["test"]   # "test" = unknown condition, for demos


def next_id(folder, label, step):
    ids = [int(p.stem.split("__")[2]) for p in folder.glob(f"{label}__{step}__*.txt")]
    return max(ids, default=-1) + 1


def flicks_from_serial(port, baud):
    import serial                                  # pip install pyserial
    # The lab board's reset/ISP pins are wired to DTR/RTS for Flash Magic.
    # Keep both released so opening the port does not hold the board in reset.
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, baud, 1
    ser.dtr = False
    ser.rts = False
    ser.open()
    with ser:
        print(f"listening on {port} at {baud} baud. Close Flash Magic first. "
              "If nothing arrives, press the board's reset switch (SW1).")
        board_res = None
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if line.startswith("RES,"):
                board_res = line[4:]
            elif line.startswith("RAW,"):
                vals = [int(v) for v in line[4:].split(",") if v]
                if len(vals) == N:
                    yield np.array(vals), board_res
                else:
                    print(f"  ignored a broken line ({len(vals)} values)")
            elif line:
                print("  board:", line)


def flicks_from_sim(label, step):
    import simulate
    rng = np.random.default_rng(int(time.time()))
    while True:
        time.sleep(0.3)
        yield simulate.flick(label, step, rng), None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="e.g. COM5 on Windows, /dev/ttyUSB0 on Linux")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--label", required=True, choices=LABELS)
    ap.add_argument("--step", type=int, default=0, help="severity step, see config.DAMAGE_LEVEL")
    ap.add_argument("--count", type=int, default=25)
    ap.add_argument("--sim", action="store_true", help="fake flicks, no board needed")
    ap.add_argument("--model", default=None, help="model set for live answers (default: real if trained, else sim)")
    args = ap.parse_args()

    dset = "sim_session" if args.sim else "real"
    folder = DATA / dset
    folder.mkdir(parents=True, exist_ok=True)
    log = DATA / f"{dset}_log.csv"

    predictor = None
    from config import RESULTS
    model = args.model or ("real" if (RESULTS / "real" / "models.pkl").exists() else "sim")
    if (RESULTS / model / "models.pkl").exists():
        from predict import Predictor
        predictor = Predictor(model)
        print(f"live answers from the '{model}' model")

    src = flicks_from_sim(args.label, args.step) if args.sim else flicks_from_serial(args.port, args.baud)
    new_log = not log.exists()
    with open(log, "a", newline="") as fh:
        cols = (["time", "file", "label", "step"] + FEATURES +
                ["tree", "nn", "nn_conf", "knn", "health", "change", "novelty",
                 "changed", "unknown", "unsafe", "board_res"])
        wr = csv.DictWriter(fh, fieldnames=cols, restval="", extrasaction="ignore")
        if new_log:
            wr.writeheader()
        for k, (adc, board_res) in enumerate(src):
            i = next_id(folder, args.label, args.step)
            path = folder / f"{args.label}__{args.step}__{i:03d}.txt"
            np.savetxt(path, adc, fmt="%d")
            row = {"time": datetime.now().isoformat(timespec="seconds"),
                   "file": path.name, "label": args.label, "step": args.step}
            if predictor:
                row.update(predictor(adc))
            row["board_res"] = board_res or ""
            wr.writerow(row)
            fh.flush()
            msg = f"[{k + 1}/{args.count}] saved {path.name}"
            if predictor:
                msg += (f"  f1={row['f1_hz']:.2f} Hz  -> {row['nn'].upper()} "
                        f"health {row['health']:.0f}%" + ("  UNKNOWN!" if row["unknown"] else ""))
            print(msg)
            if k + 1 >= args.count:
                break


if __name__ == "__main__":
    main()
