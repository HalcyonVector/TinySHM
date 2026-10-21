/* Feature extraction. Line-by-line twin of pc/features.py. */
#include <math.h>
#include "config.h"
#include "features.h"

#define PI 3.14159265f

static float re[N];
static float im[N];

/* In-place iterative radix-2 FFT (no library needed, works in Keil 4) */
static void fft(float *xr, float *xi)
{
    int i, j, k, len, half;
    float tr, ti, wr, wi, ang;

    /* bit-reversal reordering */
    j = 0;
    for (i = 0; i < N - 1; i++) {
        if (i < j) {
            tr = xr[i]; xr[i] = xr[j]; xr[j] = tr;
            ti = xi[i]; xi[i] = xi[j]; xi[j] = ti;
        }
        k = N >> 1;
        while (k <= j) { j -= k; k >>= 1; }
        j += k;
    }
    /* butterflies */
    for (len = 2; len <= N; len <<= 1) {
        half = len >> 1;
        for (k = 0; k < half; k++) {
            ang = -2.0f * PI * (float)k / (float)len;
            wr = cosf(ang);
            wi = sinf(ang);
            for (i = k; i < N; i += len) {
                j = i + half;
                tr = wr * xr[j] - wi * xi[j];
                ti = wr * xi[j] + wi * xr[j];
                xr[j] = xr[i] - tr;  xi[j] = xi[i] - ti;
                xr[i] += tr;         xi[i] += ti;
            }
        }
    }
}

void extract_features(const unsigned short *adc, float *feat)
{
    int i, n, kp, k_lo, k_hi, hw, nwin, i0, i_end, j, cnt;
    float mean, v, amp, df, a, b, c, den, delta, f1;
    float total, mainp, p, rmax, slope, tm, ym, sxx, sxy, t, y, sigma;
    static float rms[N / ENV_WIN];

    /* 1. remove offset, convert to g */
    mean = 0.0f;
    for (n = 0; n < N; n++) mean += (float)adc[n];
    mean /= (float)N;
    amp = 0.0f;
    for (n = 0; n < N; n++) {
        re[n] = ((float)adc[n] - mean) / COUNTS_PER_G;
        v = fabsf(re[n]);
        if (v > amp) amp = v;
    }

    /* 5. envelope (done before the FFT overwrites re[]) */
    nwin = N / ENV_WIN;
    for (i = 0; i < nwin; i++) {
        float s = 0.0f;
        for (n = i * ENV_WIN; n < (i + 1) * ENV_WIN; n++) s += re[n] * re[n];
        rms[i] = sqrtf(s / (float)ENV_WIN);
    }
    i0 = 0;
    for (i = 1; i < nwin; i++) if (rms[i] > rms[i0]) i0 = i;
    rmax = rms[i0];
    i_end = i0;
    while (i_end + 1 < nwin && rms[i_end + 1] > 0.05f * rmax) i_end++;
    if (i_end - i0 < 2) { i_end = i0 + 2; if (i_end > nwin - 1) i_end = nwin - 1; }
    cnt = i_end - i0 + 1;
    tm = 0.0f; ym = 0.0f;
    for (i = i0; i <= i_end; i++) {
        tm += (float)i * ENV_WIN / FS;
        ym += logf(rms[i] > 1e-6f ? rms[i] : 1e-6f);
    }
    tm /= (float)cnt; ym /= (float)cnt;
    sxx = 0.0f; sxy = 0.0f;
    for (i = i0; i <= i_end; i++) {
        t = (float)i * ENV_WIN / FS - tm;
        y = logf(rms[i] > 1e-6f ? rms[i] : 1e-6f) - ym;
        sxx += t * t; sxy += t * y;
    }
    slope = sxy / sxx;
    sigma = -slope;
    j = i0;
    while (j < nwin && rms[j] >= 0.1f * rmax) j++;
    feat[F_DECAY] = (float)(j - i0) * ENV_WIN / FS;

    /* 2. Hann window + FFT */
    for (n = 0; n < N; n++) {
        re[n] *= 0.5f - 0.5f * cosf(2.0f * PI * (float)n / (float)(N - 1));
        im[n] = 0.0f;
    }
    fft(re, im);
    for (i = 0; i <= N / 2; i++) re[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);  /* magnitude */

    /* 3. peak + parabolic interpolation */
    df = (float)FS / (float)N;
    k_lo = (int)ceilf(F_LO / df);
    k_hi = (int)floorf(F_HI / df);
    kp = k_lo;
    for (i = k_lo + 1; i <= k_hi; i++) if (re[i] > re[kp]) kp = i;
    a = re[kp - 1]; b = re[kp]; c = re[kp + 1];
    den = a - 2.0f * b + c;
    delta = (den != 0.0f) ? 0.5f * (a - c) / den : 0.0f;
    f1 = ((float)kp + delta) * df;

    /* 4. energy ratio */
    hw = (int)(PEAK_HALF_WIDTH_HZ / df + 0.5f);
    total = 0.0f; mainp = 0.0f;
    for (i = k_lo; i <= k_hi; i++) {
        p = re[i] * re[i];
        total += p;
        if (i >= kp - hw && i <= kp + hw) mainp += p;
    }

    feat[F_FREQ] = f1;
    feat[F_DAMP] = (f1 > 0.0f) ? sigma / (2.0f * PI * f1) : 0.0f;
    feat[F_AMP] = amp;
    feat[F_ENERGY] = (total > 0.0f) ? (total - mainp) / total : 0.0f;
}

/* ------------------------------------------------------------------
   Synopsis objective 5: three ways to estimate the natural frequency,
   so their accuracy, run time and memory can be compared on the board.
   Each one works on the raw samples on its own. They reuse re[]/im[],
   so call them after extract_features(). Twins: pc/freq_methods.py   */

static void remove_mean_g(const unsigned short *adc, float *x)
{
    int n;
    float mean = 0.0f;
    for (n = 0; n < N; n++) mean += (float)adc[n];
    mean /= (float)N;
    for (n = 0; n < N; n++) x[n] = ((float)adc[n] - mean) / COUNTS_PER_G;
}

/* Methods 1 and 2: FFT peak bin, optionally refined by parabolic
   interpolation. Needs re[] + im[] = 2 x N floats = 8 KB of RAM. */
float freq_fft(const unsigned short *adc, int interpolate)
{
    int i, n, kp, k_lo, k_hi;
    float df, a, b, c, den, delta;

    remove_mean_g(adc, re);
    for (n = 0; n < N; n++) {
        re[n] *= 0.5f - 0.5f * cosf(2.0f * PI * (float)n / (float)(N - 1));
        im[n] = 0.0f;
    }
    fft(re, im);
    for (i = 0; i <= N / 2; i++) re[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    df = (float)FS / (float)N;
    k_lo = (int)ceilf(F_LO / df);
    k_hi = (int)floorf(F_HI / df);
    kp = k_lo;
    for (i = k_lo + 1; i <= k_hi; i++) if (re[i] > re[kp]) kp = i;
    if (!interpolate) return (float)kp * df;
    a = re[kp - 1]; b = re[kp]; c = re[kp + 1];
    den = a - 2.0f * b + c;
    delta = (den != 0.0f) ? 0.5f * (a - c) / den : 0.0f;
    return ((float)kp + delta) * df;
}

/* Method 3: zero-crossing counting. Counts sign changes with a small
   hysteresis band (so sensor noise after the ring-down is ignored), then
   frequency = (crossings - 1) / 2 / (time from first to last crossing).
   Needs no buffer of its own beyond the samples. */
float freq_zero_crossing(const unsigned short *adc)
{
    int n, state = 0, count = 0, first = -1, last = -1;
    float mean = 0.0f, v, band = ZC_BAND_G * COUNTS_PER_G;

    for (n = 0; n < N; n++) mean += (float)adc[n];
    mean /= (float)N;
    for (n = 0; n < N; n++) {
        v = (float)adc[n] - mean;
        if (state <= 0 && v > band) {
            if (state < 0) { count++; if (first < 0) first = n; last = n; }
            state = 1;
        } else if (state >= 0 && v < -band) {
            if (state > 0) { count++; if (first < 0) first = n; last = n; }
            state = -1;
        }
    }
    if (count < 3 || last <= first) return 0.0f;
    return 0.5f * (float)(count - 1) * (float)FS / (float)(last - first);
}
