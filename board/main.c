/* Vibration-based damage detection on the LPC1768 (ALS lab board).

   Flick the ruler -> the board records 10 s of vibration, extracts 5
   features, and runs the models from model.h:
     decision tree, neural network, kNN (with teach mode), health score,
     change detector and unknown-fault detector.
   Results go to the LCD and to the PC over UART (pc/logger.py).

   Keypad:  0 = normal mode
            A = teach a new condition (next 5 flicks)
            C = self-calibrate the healthy baseline (next 10 flicks)
            F = toggle LCD page (answer / numbers)                      */
#include <LPC17xx.h>
#include "config.h"
#include "hw.h"
#include "features.h"
#include "classify.h"

enum { MODE_RUN, MODE_TEACH, MODE_CALIB };

static char line[17];

/* tiny formatter: appends text / numbers into line[] */
static int pos;
static void ln_start(void) { pos = 0; line[0] = 0; }
static void ln_str(const char *s) { while (*s && pos < 16) line[pos++] = *s++; line[pos] = 0; }
static void ln_int(long v, int width)
{
    char b[12];
    int i = 0;
    if (v < 0) { ln_str("-"); v = -v; }
    do { b[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i < width) b[i++] = '0';
    while (i && pos < 16) line[pos++] = b[--i];
    line[pos] = 0;
}
static void ln_fix(float v, int decimals)          /* fixed-point without printf */
{
    long scale = 1, iv;
    int d;
    for (d = 0; d < decimals; d++) scale *= 10;
    if (v < 0) { ln_str("-"); v = -v; }
    iv = (long)(v * scale + 0.5f);
    ln_int(iv / scale, 1);
    if (decimals) { ln_str("."); ln_int(iv % scale, decimals); }
}

static void send_results(const float *f, const Result *r)
{
    uart_puts("RES,");
    uart_puts(class_name(r->nn));   uart_putc(',');
    uart_putint((long)(r->nn_conf * 100)); uart_putc(',');
    uart_puts(class_name(r->tree)); uart_putc(',');
    uart_puts(class_name(r->knn));  uart_putc(',');
    uart_putint((long)(r->health + 0.5f)); uart_putc(',');
    uart_putint((long)(f[F_FREQ] * 1000)); uart_putc(',');   /* mHz */
    uart_putint((long)(f[F_DAMP] * 100000)); uart_putc(','); /* x1e5 */
    uart_putint(r->changed); uart_putc(',');
    uart_putint(r->unknown); uart_putc(',');
    uart_putint(r->unsafe);
    uart_puts("\r\n");
}

static void send_raw(void)
{
    int i;
    uart_puts("RAW,");
    for (i = 0; i < N; i++) {
        uart_putint(samples[i]);
        if (i < N - 1) uart_putc(',');
    }
    uart_puts("\r\n");
}

int main(void)
{
    float f[5];
    Result r;
    int mode = MODE_RUN, count = 0, page = 0, key, last_key = -1, teach_id = -1;

    SystemInit();
    SystemCoreClockUpdate();
    uart_init();
    lcd_init();
    keypad_init();
    classify_init();

#if SENSOR == SENSOR_MPU6050
    key = sensor_init();
    if (key < 0) {                      /* no answer on I2C: show it and stop */
        uart_puts("\r\nMPU6050 NOT FOUND. Check SDA/SCL/VCC/GND.\r\n");
        lcd_line(0, "MPU6050 NOT");
        lcd_line(1, "CHECK WIRING");
        while (1);
    }
    uart_puts("\r\nMPU6050 found, WHO_AM_I = "); uart_putint(key); uart_puts(" (104 = genuine)\r\n");
#else
    sensor_init();
#endif
    sampler_init();

    uart_puts("Damage detector ready. Flick the ruler.\r\n");
    lcd_line(0, "DAMAGE DETECTOR");
    lcd_line(1, "Flick the ruler");
    sampler_arm();

    while (1) {
        /* ---- keypad (acts once per press) ---- */
        key = keypad_get();
        if (key != last_key && key >= 0) {
            if (key == 0) {
                mode = MODE_RUN;
                lcd_line(0, "NORMAL MODE"); lcd_line(1, "Flick the ruler");
            } else if (key == 10) {
                teach_id = teach_new_class();
                if (teach_id < 0) { lcd_line(0, "TEACH MEMORY"); lcd_line(1, "FULL"); }
                else {
                    mode = MODE_TEACH; count = 0;
                    ln_start(); ln_str("TEACH "); ln_str(class_name(teach_id));
                    lcd_line(0, line);
                    lcd_line(1, "Flick 5 times");
                }
            } else if (key == 12) {
                mode = MODE_CALIB; count = 0;
                calib_start();
                lcd_line(0, "CALIBRATE");
                lcd_line(1, "10 healthy flick");
            } else if (key == 15) {
                page = !page;
            }
        }
        last_key = key;

        if (!sampler_done()) continue;

        /* ---- a flick was recorded ---- */
        lcd_line(1, "Thinking...");
        extract_features(samples, f);

        if (mode == MODE_TEACH) {
            teach_add(f, teach_id);
            count++;
            ln_start(); ln_str("Taught "); ln_int(count, 1); ln_str("/"); ln_int(TEACH_FLICKS, 1);
            lcd_line(1, line);
            if (count >= TEACH_FLICKS) { mode = MODE_RUN; lcd_line(0, "LEARNED!"); }
        } else if (mode == MODE_CALIB) {
            count = calib_add(f);
            ln_start(); ln_str("Calib "); ln_int(count, 1); ln_str("/"); ln_int(CALIB_FLICKS, 1);
            lcd_line(1, line);
            if (count >= CALIB_FLICKS) { calib_finish(); mode = MODE_RUN; lcd_line(0, "BASELINE SET"); }
        } else {
            classify(f, &r);
            if (r.unknown) {
                lcd_line(0, "UNKNOWN CHANGE!");
            } else {
                ln_start(); ln_str(class_name(class_is_taught(r.knn) ? r.knn : r.nn));  /* taught classes only exist in kNN */
                lcd_line(0, line);
            }
            ln_start();
            if (page == 0) {
                ln_str("H:"); ln_int((long)(r.health + 0.5f), 1); ln_str("% ");
                ln_str(r.unsafe ? "UNSAFE" : (r.changed ? "CHANGED" : "OK"));
            } else {
                ln_fix(f[F_FREQ], 2); ln_str("Hz z"); ln_fix(f[F_DAMP] * 100, 2);
            }
            lcd_line(1, line);
            send_results(f, &r);
        }
        send_raw();                     /* the laptop always gets the raw flick */
        sampler_arm();
    }
}
