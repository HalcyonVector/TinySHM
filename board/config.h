/* Shared settings. These MUST match pc/config.py. */
#ifndef CONFIG_H
#define CONFIG_H

#define FS            100      /* samples per second */
#define N             1024     /* samples per flick (10.24 s) */
#define LOG2N         10
#define COUNTS_PER_G  372.3636f /* 0.300 V/g / 3.3 V * 4096 */
#define F_LO          3.0f
#define F_HI          40.0f
#define PEAK_HALF_WIDTH_HZ 1.0f
#define ENV_WIN       25       /* 0.25 s envelope windows */

/* ---- Lab board wiring (ALS-SDA-ARMCTXM3-01) ----
   Option 1 (default): sensor on AD0.4 = P1.30. Remove the JP18 jumper and
     wire the ADXL335 Z output to the JP18 pin that carries P1.30 (see README).
     LCD on CND->CNAD and keypad on CNB->CNB3 both work.
   Option 2 (fallback): sensor on AD0.0 = P0.23 = CND pin 1. The LCD also
     needs CND, so set USE_LCD to 0 and read results on the laptop.        */
#define ADC_CH        4        /* 4 = AD0.4 (P1.30), 0 = AD0.0 (P0.23) */
#define USE_LCD       1
#define USE_KEYPAD    1
#define BAUD          115200

#define TRIGGER_G     0.30f    /* start recording when the signal jumps this much */
#define HOLD_S        2        /* ignore the beam for this long after a flick */
#define TEACH_FLICKS  5
#define CALIB_FLICKS  10

#endif
