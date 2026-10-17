/* Shared settings. These MUST match pc/config.py. */
#ifndef CONFIG_H
#define CONFIG_H

#define FS            100      /* samples per second */
#define N             1024     /* samples per flick (10.24 s) */
#define LOG2N         10

/* ---- Which accelerometer is fitted ----
   SENSOR_MPU6050 (default): GY-521 board on I2C1, read digitally.
     Samples are the 16-bit Z reading + 32768, at +-4 g (8192 counts per g).
   SENSOR_ADXL335: GY-61 board, Z output into the ADC (12-bit).
   Change SENSOR here AND in pc/config.py, then retrain.                  */
#define SENSOR_ADXL335 0
#define SENSOR_MPU6050 1
#define SENSOR        SENSOR_MPU6050

#if SENSOR == SENSOR_MPU6050
#define COUNTS_PER_G  8192.0f   /* MPU6050 at +-4 g */
#else
#define COUNTS_PER_G  372.3636f /* 0.300 V/g / 3.3 V * 4096 */
#endif
#define F_LO          3.0f
#define F_HI          40.0f
#define PEAK_HALF_WIDTH_HZ 1.0f
#define ENV_WIN       25       /* 0.25 s envelope windows */

/* ---- Lab board wiring (ALS-SDA-ARMCTXM3-01) ----
   MPU6050: SDA -> P0.19 (SDA1), SCL -> P0.20 (SCL1), both on connector CNC.
     GND -> pin 10 of any FRC connector. VCC -> 3.3 V or 5 V (GY-521 has a
     regulator and its own pull-up resistors). LCD and keypad both work.
   ADXL335 option 1 (default): sensor on AD0.4 = P1.30. Remove the JP18 jumper and
     wire the ADXL335 Z output to the JP18 pin that carries P1.30 (see README).
     LCD on CND->CNAD and keypad on CNB->CNB3 both work.
   Option 2 (fallback): sensor on AD0.0 = P0.23 = CND pin 1. The LCD also
     needs CND, so set USE_LCD to 0 and read results on the laptop.        */
#define ADC_CH        4        /* ADXL335 only: 4 = AD0.4 (P1.30), 0 = AD0.0 (P0.23) */
#define MPU_ADDR      0x68     /* MPU6050 I2C address (AD0 pin low, the GY-521 default) */
#define I2C_HZ        100000   /* 100 kHz: slow and safe over jumper wires */
#define USE_LCD       1
#define USE_KEYPAD    1
#define BAUD          115200

#define TRIGGER_G     0.30f    /* start recording when the signal jumps this much */
#define HOLD_S        2        /* ignore the beam for this long after a flick */
#define TEACH_FLICKS  5
#define CALIB_FLICKS  10

#endif
