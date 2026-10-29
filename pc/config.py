"""Shared settings. These MUST match board/config.h."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
RESULTS = ROOT / "results"
BOARD = ROOT / "board"

FS = 100            # sampling rate, Hz (TIMER0 reads the MPU6050 100 times a second)
N = 1024            # samples per flick (10.24 s, gives 0.098 Hz FFT resolution)
# MPU6050 Z reading, stored as value + 32768 (must match board/config.h)
ADC_MAX = 65535         # 16-bit reading
ZERO_G = 32768
COUNTS_PER_G = 8192.0   # +-4 g range

F_LO, F_HI = 3.0, 40.0    # search band for the main peak, Hz
PEAK_HALF_WIDTH_HZ = 1.0  # bins within this of the peak count as "main peak" energy
ENV_WIN = 25              # envelope window, samples (0.25 s)
ZC_BAND_G = 0.03          # zero-crossing hysteresis band, g (freq_methods.py)

CLASSES = ["healthy", "loose_support", "loose_joint", "added_mass"]
FEATURES = ["f1_hz", "damping"]

# Damage level used for the health-score regression (0 = perfect, 1 = worst we test).
# Added mass is deliberately 0: a coin is a change, not damage.
# Keys are "<class>:<step>" where step is what you write when recording.
DAMAGE_LEVEL = {
    "healthy:0": 0.0,
    "loose_support:1": 0.25,   # G-clamp loosened about 1/4 turn
    "loose_support:2": 0.50,   # about 1/2 turn
    "loose_support:3": 0.75,   # loosest the beam still holds (about 3/4 turn)
    "loose_joint:1": 0.50,   # middle binder clip removed
    "loose_joint:2": 1.00,   # also one end clip removed (1 clip left)
    "added_mass:1": 0.50,    # Rs 20 + Rs 5 coins taped near the tip (extra load lowers health)
}

# Recorded, but kept out of training. Lab result (2026-10-28): the first two
# loosening steps of the screw clamp leave f1 AND damping inside the healthy
# spread, so they are below what this sensor can detect. They are still plotted
# and checked against the trained model (see train.py "below detection").
TRAIN_EXCLUDE = {"loose_support:1", "loose_support:2"}
# Calibration baseline the models are trained against: frequency is divided by
# BASE_F1 and damping has BASE_DAMP subtracted, so a healthy beam reads 1.0 and 0.0.
# The board and the logger replace these with the 10-flick calibration median.
BASE_F1 = 4.739
BASE_DAMP = 0.0291
UNSAFE_HEALTH = 40.0      # below this health score the board says UNSAFE
