/* Shared settings. These MUST match pc/config.py. */
#ifndef CONFIG_H
#define CONFIG_H

#define FS            100      /* samples per second */
#define N             1024     /* samples per flick (10.24 s) */
#define LOG2N         10

/* ---- Accelerometer: MPU6050 (GY-521 board) on I2C1 ----
   Samples are the 16-bit Z reading + 32768, at +-4 g (8192 counts per g). */
#define COUNTS_PER_G  8192.0f   /* MPU6050 at +-4 g */
#define F_LO          3.0f
#define F_HI          40.0f
#define PEAK_HALF_WIDTH_HZ 1.0f
#define ENV_WIN       25       /* 0.25 s envelope windows */
#define ZC_BAND_G     0.03f    /* zero-crossing hysteresis, g */

/* ---- Lab board wiring (ALS-SDA-ARMCTXM3-01) ----
   MPU6050: SDA -> P0.19 (SDA1), SCL -> P0.20 (SCL1), middle pair of CNC.
     VCC -> TP2 (3.3 V), GND -> TP3. The GY-521 has its own regulator and
     pull-up resistors. LCD (CND->CNAD), keypad (CNB->CNB3) and LEDs
     (CNA->CNA1) all work alongside it.                                    */
#define MPU_ADDR      0x68     /* MPU6050 I2C address (AD0 pin low, the GY-521 default) */
#define I2C_HZ        100000   /* 100 kHz: slow and safe over jumper wires */
#define USE_LCD       1
#define USE_KEYPAD    1
#define USE_LEDS      1        /* health bar + alarm on LD2-LD9: FRC cable CNA -> CNA1 */
#define USE_BENCH     1        /* send a BENCH line comparing the 3 frequency methods */
#define BAUD          115200

#define TRIGGER_G     0.30f    /* start recording when the signal jumps this much */
#define HOLD_S        2        /* ignore the beam for this long after a flick */
#define TEACH_FLICKS  5
#define CALIB_FLICKS  10

#endif
