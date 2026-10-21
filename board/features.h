#ifndef FEATURES_H
#define FEATURES_H

/* Order matches pc/config.py FEATURES */
#define F_FREQ    0   /* natural frequency, Hz */
#define F_DAMP    1   /* damping ratio */
#define F_AMP     2   /* peak amplitude, g */
#define F_ENERGY  3   /* energy outside the main peak, 0..1 */
#define F_DECAY   4   /* time to drop to 10 %, s */

/* adc: N raw 12-bit samples. feat: 5 outputs. Uses about 8 KB of static RAM. */
void extract_features(const unsigned short *adc, float *feat);

/* Frequency-method comparison (synopsis objective 5). Call after
   extract_features(): they reuse its buffers. */
float freq_fft(const unsigned short *adc, int interpolate);  /* 0 = peak bin, 1 = interpolated */
float freq_zero_crossing(const unsigned short *adc);

#endif
