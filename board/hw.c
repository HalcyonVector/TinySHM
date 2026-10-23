/* Low-level drivers for the ALS-SDA-ARMCTXM3-01 lab board.
   Pin usage follows the lab manual (Labs 9 and 10, Appendix C). */
#include <LPC17xx.h>
#include "config.h"
#include "hw.h"

/* ------------------------------------------------------------------ UART0
   P0.2 TXD0 / P0.3 RXD0, the same DB9 port (CN9) Flash Magic uses. */
static unsigned int pclk_hz(void)
{
    return SystemCoreClock / 4;   /* reset default PCLKSEL = CCLK/4 */
}

void uart_init(void)
{
    unsigned int pclk = pclk_hz(), best_dl = 1, best_mul = 1, best_add = 0;
    unsigned int mul, add, dl;
    long err, best_err = 0x7fffffff, actual;

    LPC_SC->PCONP |= (1 << 3);
    LPC_PINCON->PINSEL0 &= ~0x000000F0;
    LPC_PINCON->PINSEL0 |= 0x00000050;            /* P0.2 = TXD0, P0.3 = RXD0 */

    /* pick divisor + fractional divider with the smallest baud error */
    for (mul = 1; mul <= 15; mul++) {
        for (add = 0; add < mul; add++) {
            dl = (unsigned int)((float)pclk * mul / (16.0f * BAUD * (mul + add)) + 0.5f);
            if (dl == 0) continue;
            actual = (long)((float)pclk * mul / (16.0f * dl * (mul + add)));
            err = actual - BAUD;
            if (err < 0) err = -err;
            if (err < best_err) { best_err = err; best_dl = dl; best_mul = mul; best_add = add; }
        }
    }
    LPC_UART0->LCR = 0x83;                        /* 8N1, DLAB = 1 */
    LPC_UART0->DLL = best_dl & 0xFF;
    LPC_UART0->DLM = (best_dl >> 8) & 0xFF;
    LPC_UART0->FDR = (best_mul << 4) | best_add;
    LPC_UART0->LCR = 0x03;                        /* DLAB = 0 */
    LPC_UART0->FCR = 0x07;                        /* enable + reset FIFOs */
}

void uart_putc(char c)
{
    while (!(LPC_UART0->LSR & 0x20));             /* wait for THR empty */
    LPC_UART0->THR = c;
}

void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

void uart_putint(long v)
{
    char buf[12];
    int i = 0;
    if (v < 0) { uart_putc('-'); v = -v; }
    do { buf[i++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (i) uart_putc(buf[--i]);
}

/* ------------------------------------------------------------- sensor */
/* MPU6050 on P0.19 = SDA, P0.20 = SCL (connector CNC pins 5 and 6).
   The I2C is done in software ("bit-banged") on these two GPIO pins
   instead of with the I2C1 peripheral: every step is plain code, so a
   glitch can't leave the peripheral in a stuck state. A line is driven
   low by making the pin an output (its output latch is 0) and released
   by making it an input, so the GY-521's 4.7k pull-ups pull it high.
   Every wait has a timeout, so bad wiring shows up as "not found"
   instead of a frozen board. */
#define SDA_BIT  (1u << 19)
#define SCL_BIT  (1u << 20)

static unsigned int i2c_loops;                  /* busy-loop count for a quarter bit */

static void i2c_delay(void)
{
    volatile unsigned int i;
    for (i = 0; i < i2c_loops; i++);
}

static void sda_low(void)  { LPC_GPIO0->FIODIR |= SDA_BIT; }
static void sda_high(void) { LPC_GPIO0->FIODIR &= ~SDA_BIT; }
static void scl_low(void)  { LPC_GPIO0->FIODIR |= SCL_BIT; }
static int  sda_read(void) { return (LPC_GPIO0->FIOPIN & SDA_BIT) != 0; }

static int scl_high(void)                       /* release SCL, wait if a chip holds it low */
{
    long t = 10000;
    LPC_GPIO0->FIODIR &= ~SCL_BIT;
    while (!(LPC_GPIO0->FIOPIN & SCL_BIT))
        if (--t == 0) return 0;
    return 1;
}

static void i2c_start(void)                     /* also works as a repeated start */
{
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    sda_low();  i2c_delay();
    scl_low();  i2c_delay();
}

static void i2c_stop(void)
{
    sda_low();  i2c_delay();
    scl_high(); i2c_delay();
    sda_high(); i2c_delay();
}

static int i2c_send(unsigned int b)             /* 1 = the chip acknowledged */
{
    int i, ack;
    for (i = 7; i >= 0; i--) {
        if (b & (1u << i)) sda_high(); else sda_low();
        i2c_delay();
        if (!scl_high()) return 0;
        i2c_delay(); i2c_delay();
        scl_low(); i2c_delay();
    }
    sda_high(); i2c_delay();                    /* let the chip pull SDA low = ACK */
    if (!scl_high()) return 0;
    i2c_delay();
    ack = !sda_read();
    i2c_delay();
    scl_low(); i2c_delay();
    return ack;
}

static int i2c_recv(int ack)
{
    int i, b = 0;
    sda_high();
    for (i = 0; i < 8; i++) {
        i2c_delay();
        if (!scl_high()) return -1;
        i2c_delay();
        b = (b << 1) | sda_read();
        i2c_delay();
        scl_low(); i2c_delay();
    }
    if (ack) sda_low(); else sda_high();        /* ACK = more bytes wanted */
    i2c_delay();
    if (!scl_high()) return -1;
    i2c_delay(); i2c_delay();
    scl_low(); i2c_delay();
    sda_high();
    return b;
}

static void i2c_recover(void)
{
    /* If the board was reset in the middle of a read, a chip may still be
       holding SDA low. Nine clock pulses let it finish, then a STOP. */
    int i;
    sda_high();
    for (i = 0; i < 9; i++) {
        scl_low(); i2c_delay(); i2c_delay();
        scl_high(); i2c_delay(); i2c_delay();
    }
    i2c_start();
    i2c_stop();
}

static unsigned int mpu_addr = MPU_ADDR;     /* 0x68, or 0x69 if AD0 is high */
char sensor_diag[17];                           /* why the sensor was not found, for the LCD */

static int mpu_write(unsigned int reg, unsigned int val)
{
    int ok;
    i2c_start();
    ok = i2c_send(mpu_addr << 1) && i2c_send(reg) && i2c_send(val);
    i2c_stop();
    return ok;
}

static int mpu_read(unsigned int reg, unsigned char *buf, int n)
{
    int i, v, ok;
    i2c_start();
    ok = i2c_send(mpu_addr << 1) && i2c_send(reg);
    if (ok) { i2c_start(); ok = i2c_send((mpu_addr << 1) | 1); }
    for (i = 0; ok && i < n; i++) {
        v = i2c_recv(i < n - 1);
        if (v < 0) ok = 0; else buf[i] = (unsigned char)v;
    }
    i2c_stop();
    return ok;
}

static char hex_digit(int v) { return (char)(v < 10 ? '0' + v : 'A' + v - 10); }

/* WHO_AM_I of the chips that behave like an MPU6050 (6050, 6500 and 9250
   families and common clones). */
static int is_mpu_id(int id)
{
    return id == 0x68 || id == 0x69 || id == 0x70 || id == 0x71 ||
           id == 0x72 || id == 0x73 || id == 0x98 || id == 0x19;
}

int sensor_init(void)
{
    unsigned char id = 0;
    int a, k, n, sda, scl, found;

    i2c_loops = SystemCoreClock / (4 * 4 * I2C_HZ);  /* ~4 cycles per loop, 4 quarters per bit */
    if (i2c_loops == 0) i2c_loops = 1;

    LPC_PINCON->PINSEL1 &= ~((3u << 6) | (3u << 8));             /* GPIO */
    LPC_GPIO0->FIODIR &= ~(SDA_BIT | SCL_BIT);                    /* released */

    /* Wiring check: with weak internal pull-downs, a pin wired to the
       GY-521 reads 1 (its 4.7k pull-ups win), a pin wired to nothing
       reads 0. */
    LPC_PINCON->PINMODE1 |= (3u << 6) | (3u << 8);               /* pull-down */
    delay_ms(5);
    sda = (LPC_GPIO0->FIOPIN & SDA_BIT) != 0;
    scl = (LPC_GPIO0->FIOPIN & SCL_BIT) != 0;

    /* From now on: no internal pull, open drain, output latch 0. */
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~((3u << 6) | (3u << 8))) | (2u << 6) | (2u << 8);
    LPC_PINCON->PINMODE_OD0 |= SDA_BIT | SCL_BIT;
    LPC_GPIO0->FIOCLR = SDA_BIT | SCL_BIT;

    delay_ms(100);                                   /* MPU6050 power-up time */
    i2c_recover();

    /* Look for the sensor at 0x69 first (AD0 tied high), then 0x68. A
       chip only counts if WHO_AM_I looks like an MPU. */
    found = 0;
    for (k = 0; k < 6 && !found; k++) {
        mpu_addr = (k & 1) ? MPU_ADDR : MPU_ADDR + 1;
        if (mpu_read(0x75, &id, 1) && is_mpu_id(id)) found = 1;
        else delay_ms(5);
    }

    if (!found) {
        /* LCD line 2: pin levels, then the address of every chip that
           answered (for example "S1C1 50 68"), so we can see what is on
           the bus. "--" = nothing answered. */
        sensor_diag[0] = 'S'; sensor_diag[1] = (char)('0' + sda);
        sensor_diag[2] = 'C'; sensor_diag[3] = (char)('0' + scl);
        n = 4;
        for (a = 0x08; a < 0x78 && n <= 13; a++) {
            i2c_start();
            k = i2c_send(a << 1);
            i2c_stop();
            if (k) {
                sensor_diag[n++] = ' ';
                sensor_diag[n++] = hex_digit(a >> 4);
                sensor_diag[n++] = hex_digit(a & 0xF);
            }
        }
        if (n == 4) { sensor_diag[n++] = ' '; sensor_diag[n++] = '-'; sensor_diag[n++] = '-'; }
        sensor_diag[n] = 0;
        mpu_addr = MPU_ADDR;
        return -1;
    }

    mpu_write(0x6B, 0x01);                           /* wake up, clock from gyro X PLL */
    delay_ms(10);
    mpu_write(0x1A, 0x03);                           /* low-pass filter 44 Hz (anti-alias for FS = 100) */
    mpu_write(0x19, 1000 / FS - 1);                  /* sample rate 1 kHz / 10 = 100 Hz */
    mpu_write(0x1C, 0x08);                           /* accelerometer range +-4 g */
    return id;
}

static unsigned int sensor_read(void)
{
    static unsigned int last = 32768;
    unsigned char b[2];
    if (mpu_read(0x3F, b, 2))                        /* ACCEL_ZOUT_H, ACCEL_ZOUT_L */
        last = (unsigned int)((short)((b[0] << 8) | b[1]) + 32768);
    return last;                                     /* on a glitch, repeat the last value */
}

/* ------------------------------------------------------- TIMER0 sampler */
unsigned short samples[N];
static volatile int state = 0;          /* 0 = waiting, 1 = recording, 2 = done, 3 = hold */
static volatile int idx = 0, hold = 0;
static volatile long base_q8 = 0;       /* running average of the at-rest reading, x256 */

void TIMER0_IRQHandler(void)
{
    unsigned int v;
    long d, trig;
    LPC_TIM0->IR = 1;
    v = sensor_read();
    trig = (long)(TRIGGER_G * COUNTS_PER_G);
    switch (state) {
    case 0:                                          /* waiting for a flick */
        d = (long)v - (base_q8 >> 8);
        if (d < 0) d = -d;
        if (base_q8 != 0 && d > trig) {
            state = 1; idx = 0;
            samples[idx++] = (unsigned short)v;
        } else {
            base_q8 = base_q8 ? base_q8 + (((long)v << 8) - base_q8) / 32 : ((long)v << 8);
        }
        break;
    case 1:                                          /* recording */
        samples[idx++] = (unsigned short)v;
        if (idx >= N) state = 2;
        break;
    case 3:                                          /* settle after a flick */
        base_q8 += (((long)v << 8) - base_q8) / 32;
        if (--hold <= 0) state = 0;
        break;
    default:
        break;
    }
}

void sampler_init(void)
{
    LPC_SC->PCONP |= (1 << 1);                       /* TIMER0 */
    LPC_TIM0->TCR = 2;                               /* reset */
    LPC_TIM0->PR = 0;
    LPC_TIM0->MR0 = pclk_hz() / FS - 1;              /* FS interrupts per second */
    LPC_TIM0->MCR = 3;                               /* interrupt + reset on MR0 */
    NVIC_EnableIRQ(TIMER0_IRQn);
    LPC_TIM0->TCR = 1;                               /* go */
}

void sampler_arm(void)
{
    hold = HOLD_S * FS;
    state = 3;
}

int sampler_done(void)
{
    return state == 2;
}

/* ------------------------------------------------------------------ LCD
   16x2, 4-bit mode. CND -> CNAD: data P0.23-P0.26, RS P0.27, EN P0.28.
   Short JP16 (and JP5 per the lab manual), POT3 = contrast. */
#define LCD_DATA (0xFu << 23)
#define LCD_RS   (1u << 27)
#define LCD_EN   (1u << 28)

void delay_ms(unsigned int ms)
{
    volatile unsigned int i;
    while (ms--) for (i = 0; i < SystemCoreClock / 8000; i++);
}

static void lcd_nibble(unsigned int nib, int rs)
{
    LPC_GPIO0->FIOCLR = LCD_DATA | LCD_RS;
    LPC_GPIO0->FIOSET = ((nib & 0xF) << 23) | (rs ? LCD_RS : 0);
    LPC_GPIO0->FIOSET = LCD_EN;
    delay_ms(1);
    LPC_GPIO0->FIOCLR = LCD_EN;
    delay_ms(1);
}

static void lcd_byte(unsigned int b, int rs)
{
    lcd_nibble(b >> 4, rs);
    lcd_nibble(b, rs);
    if (!rs && b < 4) delay_ms(2);                   /* clear / home are slow */
}

void lcd_init(void)
{
#if USE_LCD
    LPC_PINCON->PINSEL1 &= ~(0x3FFu << 14);          /* P0.23..P0.27 GPIO */
    LPC_PINCON->PINSEL1 &= ~(0x3u << 24);            /* P0.28 GPIO */
    LPC_GPIO0->FIODIR |= LCD_DATA | LCD_RS | LCD_EN;
    delay_ms(20);
    lcd_nibble(0x3, 0); delay_ms(5);
    lcd_nibble(0x3, 0); delay_ms(1);
    lcd_nibble(0x3, 0);
    lcd_nibble(0x2, 0);                              /* 4-bit mode */
    lcd_byte(0x28, 0);                               /* 2 lines, 5x8 */
    lcd_byte(0x0C, 0);                               /* display on, no cursor */
    lcd_byte(0x06, 0);                               /* auto increment */
    lcd_byte(0x01, 0);                               /* clear */
#endif
}

void lcd_line(int row, const char *s)
{
#if USE_LCD
    int i;
    lcd_byte(row ? 0xC0 : 0x80, 0);
    for (i = 0; i < 16; i++) lcd_byte(*s ? (unsigned char)*s++ : ' ', 1);
#else
    (void)row; (void)s;
#endif
}

/* --------------------------------------------------------------- keypad
   4x4 keys SW3-SW18. CNB -> CNB3, short JP4 (1,2).
   Rows P2.10-P2.13 (outputs), columns P1.23-P1.26 (inputs). */
void keypad_init(void)
{
#if USE_KEYPAD
    LPC_PINCON->PINSEL3 &= ~(0xFFu << 14);           /* P1.23..P1.26 GPIO */
    LPC_PINCON->PINSEL4 &= ~(0xFFu << 20);           /* P2.10..P2.13 GPIO */
    LPC_GPIO2->FIODIR |= (0xFu << 10);
    LPC_GPIO1->FIODIR &= ~(0xFu << 23);
#endif
}

int keypad_get(void)
{
#if USE_KEYPAD
    int row, col;
    unsigned int c;
    volatile int d;
    for (row = 0; row < 4; row++) {
        LPC_GPIO2->FIOCLR = (0xFu << 10);
        LPC_GPIO2->FIOSET = (1u << (10 + row));
        for (d = 0; d < 200; d++);
        c = (LPC_GPIO1->FIOPIN >> 23) & 0xF;
        if (c) {
            for (col = 0; col < 4; col++)
                if (c & (1u << col)) return row * 4 + col;
        }
    }
#endif
    return -1;
}

/* ------------------------------------------------------------- LEDs
   LD2-LD9 on P0.4-P0.11 (Lab 6): FRC cable from CNA to CNA1. */
void leds_init(void)
{
#if USE_LEDS
    LPC_PINCON->PINSEL0 &= ~(0xFFFFu << 8);          /* P0.4..P0.11 GPIO */
    LPC_GPIO0->FIODIR |= (0xFFu << 4);
    LPC_GPIO0->FIOCLR = (0xFFu << 4);
#endif
}

void leds_set(unsigned int bits)
{
#if USE_LEDS
    LPC_GPIO0->FIOCLR = (0xFFu << 4);
    LPC_GPIO0->FIOSET = (bits & 0xFFu) << 4;
#else
    (void)bits;
#endif
}

/* ------------------------------------------------- SysTick cycle timer
   Counts CPU cycles (1 cycle = 10 ns at 100 MHz) to time each
   frequency method. Wraps every 2^24 cycles; the interrupt counts wraps. */
static volatile unsigned long st_wraps;

void SysTick_Handler(void)
{
    st_wraps++;
}

void cycles_start(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 0xFFFFFFu;
    SysTick->VAL = 0;
    st_wraps = 0;
    SysTick->CTRL = 7;                               /* CPU clock, interrupt, enable */
}

unsigned long cycles_now(void)
{
    unsigned long w, v;
    do {
        w = st_wraps;
        v = SysTick->VAL;
    } while (w != st_wraps);
    return w * 0x1000000ul + (0xFFFFFFul - v);
}
