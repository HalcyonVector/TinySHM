"""Shared settings. These MUST match board/config.h."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
RESULTS = ROOT / "results"
BOARD = ROOT / "board"

FS = 100            # sampling rate, Hz (TIMER0 triggers the ADC 100 times a second)
N = 1024            # samples per flick (10.24 s, gives 0.098 Hz FFT resolution)
ADC_MAX = 4095      # 12-bit ADC
VREF = 3.3
SENS_V_PER_G = 0.300                          # ADXL335 typical sensitivity at 3.3 V
COUNTS_PER_G = SENS_V_PER_G / VREF * 4096     # about 372 ADC counts per g

F_LO, F_HI = 3.0, 40.0    # search band for the main peak, Hz
PEAK_HALF_WIDTH_HZ = 1.0  # bins within this of the peak count as "main peak" energy
ENV_WIN = 25              # envelope window, samples (0.25 s)

CLASSES = ["healthy", "loose_clamp", "loose_joint", "added_mass"]
FEATURES = ["f1_hz", "damping", "amp_g", "energy_ratio", "decay_s"]

# Damage level used for the health-score regression (0 = perfect, 1 = worst we test).
# Added mass is deliberately 0: a coin is a change, not damage.
# Keys are "<class>:<step>" where step is what you write when recording.
DAMAGE_LEVEL = {
    "healthy:0": 0.0,
    "loose_clamp:1": 0.25,   # clamp backed off 1/4 turn
    "loose_clamp:2": 0.50,   # 1/2 turn
    "loose_clamp:3": 0.75,   # 3/4 turn
    "loose_joint:1": 0.50,   # 1 binder clip removed
    "loose_joint:2": 1.00,   # 2 binder clips removed
    "added_mass:1": 0.0,     # one coin taped near the tip
}
UNSAFE_HEALTH = 40.0      # below this health score the board says UNSAFE
