"""Three ways to estimate the natural frequency (synopsis objective 5).

Python twins of freq_fft() and freq_zero_crossing() in board/features.c:
  1. FFT peak bin            (resolution FS/N = 0.098 Hz)
  2. Interpolated FFT peak   (parabola through the 3 bins around the peak)
  3. Zero-crossing counting  (no FFT at all, almost no memory)
"""
import math
import numpy as np
from config import FS, N, COUNTS_PER_G, F_LO, F_HI, ZC_BAND_G


def fft_peak(adc, interpolate):
    x = (np.asarray(adc, float) - np.mean(adc)) / COUNTS_PER_G
    n = np.arange(N)
    mag = np.abs(np.fft.rfft(x * (0.5 - 0.5 * np.cos(2 * math.pi * n / (N - 1)))))
    df = FS / N
    k_lo, k_hi = int(math.ceil(F_LO / df)), int(math.floor(F_HI / df))
    kp = k_lo + int(np.argmax(mag[k_lo:k_hi + 1]))
    if not interpolate:
        return kp * df
    a, b, c = mag[kp - 1], mag[kp], mag[kp + 1]
    den = a - 2 * b + c
    return (kp + (0.5 * (a - c) / den if den != 0 else 0.0)) * df


def zero_crossing(adc):
    v = np.asarray(adc, float) - np.mean(adc)
    band = ZC_BAND_G * COUNTS_PER_G
    state, count, first, last = 0, 0, -1, -1
    for i, s in enumerate(v):
        if state <= 0 and s > band:
            if state < 0:
                count += 1
                first = i if first < 0 else first
                last = i
            state = 1
        elif state >= 0 and s < -band:
            if state > 0:
                count += 1
                first = i if first < 0 else first
                last = i
            state = -1
    if count < 3 or last <= first:
        return 0.0
    return 0.5 * (count - 1) * FS / (last - first)


def reference(adc, pad=16):
    """Fine reference: 16x zero-padded FFT (0.006 Hz bins) + interpolation, on the PC."""
    x = (np.asarray(adc, float) - np.mean(adc)) / COUNTS_PER_G
    n = np.arange(N)
    mag = np.abs(np.fft.rfft(x * (0.5 - 0.5 * np.cos(2 * math.pi * n / (N - 1))), N * pad))
    df = FS / (N * pad)
    k_lo, k_hi = int(math.ceil(F_LO / df)), int(math.floor(F_HI / df))
    kp = k_lo + int(np.argmax(mag[k_lo:k_hi + 1]))
    a, b, c = mag[kp - 1], mag[kp], mag[kp + 1]
    den = a - 2 * b + c
    return (kp + (0.5 * (a - c) / den if den != 0 else 0.0)) * df
