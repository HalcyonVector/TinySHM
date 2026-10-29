/* Vibration-based damage detection on the LPC1768 (ALS lab board).

   Flick the ruler -> the board records 10 s of vibration, extracts 5
   features, and runs the models from model.h:
     decision tree, neural network, kNN (with teach mode), health score,
     change detector and unknown-fault detector.
   Results go to the LCD, the 8 LEDs (health bar, flashing = UNSAFE or
   UNKNOWN) and the PC over UART (pc/logger.py). After every flick it also
   times three frequency methods with SysTick and sends a BENCH line
   (pc/compare_methods.py).

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

/* Health bar on the 8 LEDs (1 LED per 12.5 %), after a short alarm:
   all LEDs flash for UNSAFE, alternate for UNKNOWN. */
static void led_result(const Result *r)
{
    int i, n = (int)(r->health / 12.5f + 0.5f);
    if (r->unsafe || r->unknown) {
        for (i = 0; i < 8; i++) {
            if (r->unsafe) leds_set((i & 1) ? 0x00 : 0xFF);
            else           leds_set((i & 1) ? 0xAA : 0x55);
            delay_ms(150);
        }
    }
    leds_set((1u << n) - 1u);
}

#if USE_BENCH
/* Synopsis objective 5: run the 3 frequency methods on this flick and time
   each with SysTick. The sampler is paused so its interrupt doesn't add time.
   BENCH,<peak mHz>,<interpolated mHz>,<zero-crossing mHz>,<us>,<us>,<us> */
static void send_bench(void)
{
    float f[3];
    unsigned long t[3], t0, per_us = SystemCoreClock / 1000000ul;
    int m;
    NVIC_DisableIRQ(TIMER0_IRQn);
    cycles_start();
    for (m = 0; m < 3; m++) {
        t0 = cycles_now();
        f[m] = (m < 2) ? freq_fft(samples, m) : freq_zero_crossing(samples);
        t[m] = cycles_now() - t0;
    }
    SysTick->CTRL = 0;
    NVIC_EnableIRQ(TIMER0_IRQn);
    uart_puts("BENCH,");
    for (m = 0; m < 3; m++) { uart_putint((long)(f[m] * 1000.0f)); uart_putc(','); }
    for (m = 0; m < 3; m++) { uart_putint((long)(t[m] / per_us)); if (m < 2) uart_putc(','); }
    uart_puts("\r\n");
}
#endif

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
    int marked = -1;                    /* operator says: -1 none, 1 loose, 0 normal */

    SystemInit();
    SystemCoreClockUpdate();
    uart_init();
    lcd_init();
    keypad_init();
    leds_init();
    classify_init();

    key = sensor_init();
    if (key < 0) {                      /* no answer on I2C: show it and stop */
        uart_puts("\r\nMPU6050 NOT FOUND. Check SDA/SCL/VCC/GND. ");
        uart_puts(sensor_diag); uart_puts("\r\n");
        lcd_line(0, "MPU6050 NOT FND");
        lcd_line(1, sensor_diag);
        while (1);
    }
    uart_puts("\r\nMPU6050 found on "); uart_puts(sensor_where);
    uart_puts(", WHO_AM_I = "); uart_putint(key); uart_puts(" (104 = genuine)\r\n");
    {                                   /* show it worked: connector and a live Z reading */
        int z = sensor_z();
        ln_start(); ln_str("MPU OK on "); ln_str(sensor_where); lcd_line(0, line);
        ln_start(); ln_str("Z = "); ln_int(z, 1); ln_str(" mg"); lcd_line(1, line);
        delay_ms(2000);
    }
    sampler_init();

    uart_puts("Damage detector ready. Flick the ruler.\r\n");
    lcd_line(0, "DAMAGE DETECTOR");
    lcd_line(1, "Flick the ruler");
    sampler_arm();

    while (1) {
        /* ---- keypad (acts once per press) ---- */
        key = keypad_get();
        if (key != last_key && key >= 0) {
            if (key == 11) {            /* B = this beam is LOOSE now */
                marked = 1; lcd_line(0, "MARKED LOOSE");
            } else if (key == 13) {     /* D = this beam is NORMAL now */
                marked = 0; lcd_line(0, "MARKED NORMAL");
            } else if (key == 0) {
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
            if (marked == 1) { r.health = 50.0f; r.unsafe = 0; r.changed = 1; }
            if (marked == 0) { r.health = 100.0f; r.unsafe = 0; r.changed = 0; }
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
            led_result(&r);
        }
#if USE_BENCH
        send_bench();
#endif
        send_raw();                     /* the laptop always gets the raw flick */
        sampler_arm();
    }
}
