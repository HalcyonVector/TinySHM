"""Turn one flick (N raw ADC counts) into 5 numbers.

This is a line-by-line twin of board/features.c, so the PC and the board
compute the same features. If you change one, change the other and rerun
`python pc/check_c_parity.py`.
"""
import math
import numpy as np
from config import FS, N, COUNTS_PER_G, F_LO, F_HI, PEAK_HALF_WIDTH_HZ, ENV_WIN


def extract(adc):
    adc = np.asarray(adc, dtype=np.float64)
    assert len(adc) == N, f"expected {N} samples, got {len(adc)}"

    # 1. Remove the 1.65 V offset and convert to g
    x = (adc - adc.mean()) / COUNTS_PER_G
    amp_g = float(np.max(np.abs(x)))

    # 2. Hann window + FFT, magnitude spectrum
    n = np.arange(N)
    w = 0.5 - 0.5 * np.cos(2 * math.pi * n / (N - 1))
    mag = np.abs(np.fft.rfft(x * w))
    df = FS / N
    k_lo = int(math.ceil(F_LO / df))
    k_hi = int(math.floor(F_HI / df))

    # 3. Peak frequency with parabolic interpolation between bins
    kp = k_lo + int(np.argmax(mag[k_lo:k_hi + 1]))
    a, b, c = mag[kp - 1], mag[kp], mag[kp + 1]
    den = a - 2 * b + c
    delta = 0.5 * (a - c) / den if den != 0 else 0.0
    f1 = (kp + delta) * df

    # 4. Energy ratio: how much of the vibration is NOT the main peak (rattle, harmonics)
    p = mag ** 2
    hw = int(round(PEAK_HALF_WIDTH_HZ / df))
    total = p[k_lo:k_hi + 1].sum()
    main = p[max(k_lo, kp - hw):min(k_hi, kp + hw) + 1].sum()
    energy_ratio = float((total - main) / total) if total > 0 else 0.0

    # 5. Envelope: RMS in 0.25 s windows, then fit a straight line to log(RMS)
    nwin = N // ENV_WIN
    rms = np.sqrt((x[:nwin * ENV_WIN].reshape(nwin, ENV_WIN) ** 2).mean(axis=1))
    i0 = int(np.argmax(rms))
    rmax = rms[i0]
    i_end = i0
    while i_end + 1 < nwin and rms[i_end + 1] > 0.05 * rmax:
        i_end += 1
    if i_end - i0 < 2:
        i_end = min(nwin - 1, i0 + 2)
    t = np.arange(i0, i_end + 1) * (ENV_WIN / FS)
    y = np.log(np.maximum(rms[i0:i_end + 1], 1e-6))
    tm, ym = t.mean(), y.mean()
    slope = ((t - tm) * (y - ym)).sum() / ((t - tm) ** 2).sum()
    sigma = -slope
    damping = float(sigma / (2 * math.pi * f1)) if f1 > 0 else 0.0

    # 6. Decay time: time from the loudest window until RMS drops below 10 %
    j = i0
    while j < nwin and rms[j] >= 0.1 * rmax:
        j += 1
    decay_s = (j - i0) * ENV_WIN / FS

    return [float(f1), damping, amp_g, energy_ratio, float(decay_s)]


def load_raw(path):
    """A raw flick file is one ADC count per line (what logger.py saves)."""
    return np.loadtxt(path, dtype=np.int32)
