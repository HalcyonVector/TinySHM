"""Record flicks from the board in the lab.

The board sends one line per flick:  RAW,<1024 raw readings>
(and, once the model is on it, a RES,... line with its own answer).

Usage (in the lab):
  python pc/logger.py --port COM5 --label healthy --step 0 --count 25
  python pc/logger.py --port COM5 --label loose_support --step 1 --count 12
All conditions in one go, with the port opened only once (it walks you
through the table in BUILD_GUIDE Part 6 and skips flicks already recorded):
  python pc/logger.py --port COM5 --plan
Practice without a board:
  python pc/logger.py --sim --label loose_support --step 2 --count 3
From a terminal capture (lab PC without Python: save the serial output of
PuTTY / Tera Term to a text file, one file per condition, copy it over):
  python pc/logger.py --file healthy.txt --label healthy --step 0

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


def parse_lines(lines):
    """Turn board output lines into (readings, RES, BENCH) per flick."""
    board_res = board_bench = None
    for line in lines:
        line = line.strip()
        if line.startswith("RES,"):
            board_res = line[4:]
        elif line.startswith("BENCH,"):
            board_bench = line[6:]
        elif line.startswith("RAW,"):
            vals = [int(v) for v in line[4:].split(",") if v.strip().isdigit()]
            if len(vals) == N:
                yield np.array(vals), board_res, board_bench
                board_res = board_bench = None
            else:
                print(f"  ignored a broken line ({len(vals)} values)")
        elif line:
            print("  board:", line)


def flicks_from_file(path):
    with open(path, encoding="utf-8", errors="ignore") as fh:
        yield from parse_lines(fh)


# BUILD_GUIDE Part 6, counts cut to fit one lab slot: (label, step, flicks wanted, what to do by hand)
PLAN = [
    ("healthy", 0, 25, "HEALTHY: clamp tight, all 3 clips on, no coin"),
    ("added_mass", 1, 12, "COIN: tape a coin on ruler B next to the sensor"),
    ("loose_joint", 1, 10, "LOOSE JOINT 1: remove the coin, remove the MIDDLE clip"),
    ("loose_joint", 2, 8, "LOOSE JOINT 2: also remove one END clip (1 clip left)"),
    ("loose_support", 1, 8, "LOOSE SUPPORT 1: put all 3 clips back, loosen the clamp 1/4 turn"),
    ("loose_support", 2, 8, "LOOSE SUPPORT 2: loosen the clamp to 1/2 turn"),
    ("loose_support", 3, 8, "LOOSE SUPPORT 3: loosen to 3/4 turn (beam still held)"),
]


def open_serial(port, baud):
    import serial                                  # pip install pyserial
    # The lab board's reset/ISP pins are wired to DTR/RTS for Flash Magic.
    # Keep both released so opening the port does not hold the board in reset.
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, baud, 1
    ser.dtr = False
    ser.rts = False
    try:
        ser.open()
    except serial.SerialException as e:
        # Some CH340 USB-serial drivers reject the DTR/RTS settings
        # ("A device attached to the system is not functioning"). Retry with
        # the driver's defaults; press reset on the board if it stops.
        print(f"  port setup refused ({e}); retrying with default settings")
        try:
            ser = serial.Serial(port, baud, timeout=1)
        except serial.SerialException as e2:
            print(f"  refused again ({e2}); setting the port up with Windows 'mode'")
            ser = RawPort(port, baud)
    print(f"listening on {port} at {baud} baud. Close Flash Magic first. "
          "If nothing arrives, press the board's reset switch.")
    return ser


class RawPort:
    """Last resort for CH340 drivers that reject pyserial's settings: let
    Windows' own 'mode' command configure the port, then read it as a file."""

    def __init__(self, port, baud):
        import subprocess
        out = subprocess.run(["mode", port, f"BAUD={baud}", "PARITY=n", "DATA=8", "STOP=1",
                              "dtr=off", "rts=off"], shell=True, capture_output=True, text=True)
        print("  " + " ".join((out.stdout + out.stderr).split())[:200])
        self.fh = open("\\\\.\\" + port, "rb", buffering=0)
        try:                                   # release DTR and RTS: they hold the board in reset
            import ctypes, msvcrt
            h = msvcrt.get_osfhandle(self.fh.fileno())
            ctypes.windll.kernel32.EscapeCommFunction(h, 6)   # CLRDTR
            ctypes.windll.kernel32.EscapeCommFunction(h, 4)   # CLRRTS
        except Exception as e:
            print("  could not release DTR/RTS:", e)

    def readline(self):
        line = bytearray()
        while not line.endswith(b"\n"):
            b = self.fh.read(1)
            if not b:
                break
            line += b
        return bytes(line)

    def reset_input_buffer(self):
        pass

    def close(self):
        self.fh.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def flicks_from_port(ser):
    yield from parse_lines(ser.readline().decode(errors="ignore") for _ in iter(int, 1))


def flicks_from_serial(port, baud):
    with open_serial(port, baud) as ser:
        yield from flicks_from_port(ser)


def flicks_from_sim(label, step):
    import simulate
    rng = np.random.default_rng(int(time.time()))
    while True:
        time.sleep(0.3)
        yield simulate.flick(label, step, rng), None, None

# Lab menu (--session): what you set up for each reading, and how it is saved.
MENU = [
    ("healthy", 0, "HEALTHY: clamp tight, 3 clips on, no coin"),
    ("loose_joint", 1, "LOOSE JOINT 1: middle clip off"),
    ("loose_joint", 2, "LOOSE JOINT 2: middle and one end clip off"),
    ("loose_support", 1, "LOOSE SUPPORT 1/4 turn"),
    ("loose_support", 2, "LOOSE SUPPORT 1/2 turn"),
    ("loose_support", 3, "LOOSE SUPPORT 3/4 turn"),
    ("test", 0, "UNKNOWN: any other setup you want to check"),
]
# Coin reading first (the trained ADDED MASS class), then the same readings
# as above with no coin. Each one is saved under its own class.
MENU = [
    ("added_mass", 1, "COIN ONLY: coin on ruler B, clamp tight, 3 clips on"),
] + MENU


def ask_yes(question):
    return input(f"{question} (y/n): ").strip().lower().startswith("y")

def calibration_log(ser, args, n=10):
    """Show each calibration flick (not saved): frequency, damping, amplitude.
    The board does the baseline itself; this shows what it is learning from."""
    from features import extract
    from config import FEATURES
    if ser is not None:
        ser.reset_input_buffer()
        src = flicks_from_port(ser)
    else:
        src = flicks_from_sim("healthy", 0)
    rows = []
    for k, (adc, _, _) in enumerate(src, 1):
        f = dict(zip(["f1_hz", "damping", "amp_g", "energy_ratio", "decay_s"], extract(adc)))
        rows.append(f)
        print(f"    calib {k}/{n}: f1={f['f1_hz']:.2f} Hz  damping={f['damping']:.4f}  "
              f"amp={f['amp_g']:.2f} g  energy={f['energy_ratio']:.2f}")
        if k >= n:
            break
    f1 = np.array([r["f1_hz"] for r in rows])
    dm = np.array([r["damping"] for r in rows])
    print(f"    baseline from {len(rows)} flicks: f1 median {np.median(f1):.2f} Hz "
          f"(range {f1.min():.2f} to {f1.max():.2f}), damping median {np.median(dm):.4f}")
    input("    Check the LCD says BASELINE SET, then press Enter: ")
    return float(np.median(f1)), float(np.median(dm))


def session(args, folder, log, predictor):
    """Lab menu: calibrate?, then pick a reading, flick, see the answer."""
    ser = None if args.sim else open_serial(args.port, args.baud)
    try:
        if ask_yes("\nCalibrate first (press C, 10 flicks)?"):
            print("    Press C on the keypad, then flick 10 times. Each calibration flick is shown below.")
            base = calibration_log(ser, args)
            if predictor is not None:
                predictor.base = base     # live answers now use this beam's healthy level
        while True:
            print("\nChoose the reading you are about to flick:")
            for n, (label, step, what) in enumerate(MENU, 1):
                print(f"  {n}. {what}   [{next_id(folder, label, step)} saved]")
            print("  q. quit")
            ans = input("Number: ").strip().lower()
            if ans == "q":
                break
            if not ans.isdigit() or not 1 <= int(ans) <= len(MENU):
                print("    Pick a number from the list.")
                continue
            label, step, what = MENU[int(ans) - 1]
            count = int(input("    How many flicks for this reading? [1]: ").strip() or 1)
            input(f"    Set up: {what}. Wait for 'Flick the ruler', then press Enter: ")
            if ser is not None:
                ser.reset_input_buffer()
                src = flicks_from_port(ser)
            else:
                src = flicks_from_sim(label, step)
            record(src, label, step, count, folder, log, predictor)
    finally:
        if ser is not None:
            ser.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", help="e.g. COM5 on Windows, /dev/ttyUSB0 on Linux")
    ap.add_argument("--baud", type=int, default=19200)
    ap.add_argument("--label", choices=LABELS)
    ap.add_argument("--plan", action="store_true",
                    help="record every condition of BUILD_GUIDE Part 6 in one session")
    ap.add_argument("--step", type=int, default=0, help="severity step, see config.DAMAGE_LEVEL")
    ap.add_argument("--count", type=int, default=25)
    ap.add_argument("--sim", action="store_true", help="fake flicks, no board needed")
    ap.add_argument("--file", help="import a saved terminal capture instead of reading the port")
    ap.add_argument("--session", action="store_true",
                    help="lab menu: calibrate, add weight, pick a reading, flick, see the answer")
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

    if args.plan:
        run_plan(args, folder, log, predictor)
        return
    if args.session:
        session(args, folder, log, predictor)
        return
    if not args.label:
        ap.error("--label is required (or use --plan)")
    if args.sim:
        src = flicks_from_sim(args.label, args.step)
    elif args.file:
        src = flicks_from_file(args.file)
        args.count = 10 ** 9                       # take every flick in the file
    else:
        src = flicks_from_serial(args.port, args.baud)
    record(src, args.label, args.step, args.count, folder, log, predictor,
           show_total=not args.file)


def record(src, label, step, count, folder, log, predictor, show_total=True):
    new_log = not log.exists()
    with open(log, "a", newline="", encoding="utf-8") as fh:
        cols = (["time", "file", "label", "step"] + FEATURES +
                ["tree", "nn", "nn_conf", "knn", "health", "change", "novelty",
                 "changed", "unknown", "unsafe", "board_res", "board_bench"])
        wr = csv.DictWriter(fh, fieldnames=cols, restval="", extrasaction="ignore")
        if new_log:
            wr.writeheader()
        for k, (adc, board_res, board_bench) in enumerate(src):
            i = next_id(folder, label, step)
            path = folder / f"{label}__{step}__{i:03d}.txt"
            np.savetxt(path, adc, fmt="%d")
            row = {"time": datetime.now().isoformat(timespec="seconds"),
                   "file": path.name, "label": label, "step": step}
            if predictor:
                row.update(predictor(adc))
            row["board_res"] = board_res or ""
            row["board_bench"] = board_bench or ""
            wr.writerow(row)
            fh.flush()
            msg = f"[{k + 1}{'/' + str(count) if show_total else ''}] saved {path.name}"
            if predictor:
                msg += (f"  f1={row['f1_hz']:.2f} Hz  -> {row['nn'].upper()} "
                        f"health {row['health']:.0f}%" + ("  UNKNOWN!" if row["unknown"] else ""))
            print(msg)
            if k + 1 >= count:
                break


def mean_f1(paths):
    from features import extract, load_raw
    f1 = [extract(load_raw(p))[0] for p in paths]
    return float(np.median(f1)) if f1 else float("nan")


def setup_check(ser, args, folder, log, predictor, n=5):
    """New lab session, beam rebuilt: record a few healthy flicks and compare
    their frequency with the healthy flicks already saved."""
    before = sorted(folder.glob("healthy__0__*.txt"))
    ref = mean_f1(before)
    print(f"\n=== SETUP CHECK: beam rebuilt? Set it up HEALTHY (clamp tight, 3 clips, no coin).")
    print(f"    {n} healthy flicks; earlier healthy flicks ring at {ref:.2f} Hz.")
    ans = input("    Press C on the keypad, do the 10 calibration flicks, wait for the LCD,\n"
                "    then press Enter (s = skip the check): ").strip().lower()
    if ans == "s":
        return
    if ser is not None:
        ser.reset_input_buffer()
        src = flicks_from_port(ser)
    else:
        src = flicks_from_sim("healthy", 0)
    record(src, "healthy", 0, n, folder, log, predictor)
    now = mean_f1(sorted(set(folder.glob("healthy__0__*.txt")) - set(before)))
    diff = abs(now - ref) / ref * 100
    print(f"    today {now:.2f} Hz vs earlier {ref:.2f} Hz ({diff:.0f}% apart)")
    if diff > 5:
        print("    More than 5% off: check the clamp is tight, ruler A sits on its marks,\n"
              "    ruler B overlap is 5 cm and the sensor is 2 cm from the tip. Then rerun.")
    else:
        print("    Matches. Carry on.")


def run_plan(args, folder, log, predictor):
    """Walk through every condition with the port open the whole time."""
    ser = None if args.sim else open_serial(args.port, args.baud)
    try:
        if next_id(folder, "healthy", 0) >= PLAN[0][2]:
            setup_check(ser, args, folder, log, predictor)
        for label, step, want, what in PLAN:
            have = next_id(folder, label, step)
            left = want - have
            if left <= 0:
                print(f"\n{label} step {step}: already have {have}, skipping")
                continue
            print(f"\n=== {what}")
            print(f"    {left} flicks to record ({have} already saved).")
            ans = input("    Set it up, wait until the LCD shows 'Flick the ruler' or a result,\n"
                        "    then press Enter (s = skip, q = quit): ").strip().lower()
            if ans == "q":
                break
            if ans == "s":
                continue
            if ser is not None:
                ser.reset_input_buffer()           # drop anything the setup itself triggered
                src = flicks_from_port(ser)
            else:
                src = flicks_from_sim(label, step)
            record(src, label, step, left, folder, log, predictor)
        print("\nDone. Now: git add data/real data/real_log.csv, commit, push.")
    finally:
        if ser is not None:
            ser.close()

if __name__ == "__main__":
    main()
