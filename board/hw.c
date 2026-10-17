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
#if SENSOR == SENSOR_MPU6050
/* MPU6050 on I2C1: P0.19 = SDA1, P0.20 = SCL1 (connector CNC).
   Simple polled I2C master (UM10360 chapter 19 state codes). Every wait
   has a timeout, so loose wiring shows up as "sensor not found" instead
   of a frozen board. */
#define I2C_AA   0x04
#define I2C_SI   0x08
#define I2C_STO  0x10
#define I2C_STA  0x20
#define I2C_EN   0x40

static int i2c_wait(void)
{
    long t = 100000;
    while (!(LPC_I2C1->I2CONSET & I2C_SI))
        if (--t == 0) return -1;
    return (int)LPC_I2C1->I2STAT;
}

static int i2c_start(void)                      /* 0x08 start, 0x10 repeated start */
{
    int st;
    LPC_I2C1->I2CONSET = I2C_STA;
    LPC_I2C1->I2CONCLR = I2C_SI;
    st = i2c_wait();
    LPC_I2C1->I2CONCLR = I2C_STA;
    return st;
}

static int i2c_send(unsigned int b)             /* 0x18 / 0x28 / 0x40 = acked */
{
    LPC_I2C1->I2DAT = b & 0xFF;
    LPC_I2C1->I2CONCLR = I2C_SI;
    return i2c_wait();
}

static int i2c_recv(int ack)
{
    if (ack) LPC_I2C1->I2CONSET = I2C_AA;
    else     LPC_I2C1->I2CONCLR = I2C_AA;
    LPC_I2C1->I2CONCLR = I2C_SI;
    if (i2c_wait() < 0) return -1;
    return (int)(LPC_I2C1->I2DAT & 0xFF);
}

static void i2c_stop(void)
{
    long t = 100000;
    LPC_I2C1->I2CONSET = I2C_STO;
    LPC_I2C1->I2CONCLR = I2C_SI;
    while ((LPC_I2C1->I2CONSET & I2C_STO) && --t);
}

static int mpu_write(unsigned int reg, unsigned int val)
{
    int ok = i2c_start() == 0x08 && i2c_send(MPU_ADDR << 1) == 0x18 &&
             i2c_send(reg) == 0x28 && i2c_send(val) == 0x28;
    i2c_stop();
    return ok;
}

static int mpu_read(unsigned int reg, unsigned char *buf, int n)
{
    int i, v, ok = i2c_start() == 0x08 && i2c_send(MPU_ADDR << 1) == 0x18 &&
                   i2c_send(reg) == 0x28 && i2c_start() == 0x10 &&
                   i2c_send((MPU_ADDR << 1) | 1) == 0x40;
    for (i = 0; ok && i < n; i++) {
        v = i2c_recv(i < n - 1);
        if (v < 0) ok = 0; else buf[i] = (unsigned char)v;
    }
    i2c_stop();
    return ok;
}

int sensor_init(void)
{
    unsigned char id;
    unsigned int half = (SystemCoreClock / 4) / (2 * I2C_HZ);   /* PCLK_I2C1 = CCLK/4 */

    LPC_SC->PCONP |= (1 << 19);                      /* I2C1 power */
    LPC_PINCON->PINSEL1 |= (3u << 6) | (3u << 8);    /* P0.19 = SDA1, P0.20 = SCL1 */
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~((3u << 6) | (3u << 8))) | (2u << 6) | (2u << 8);
    LPC_PINCON->PINMODE_OD0 |= (1u << 19) | (1u << 20);  /* open drain, GY-521 has pull-ups */
    LPC_I2C1->I2CONCLR = I2C_AA | I2C_SI | I2C_STA | I2C_EN;
    LPC_I2C1->I2SCLH = half;
    LPC_I2C1->I2SCLL = half;
    LPC_I2C1->I2CONSET = I2C_EN;

    delay_ms(100);                                   /* MPU6050 power-up time */
    if (!mpu_read(0x75, &id, 1)) return -1;          /* WHO_AM_I: 0x68 on a real MPU6050 */
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

#else
/* ADXL335 Z output on the ADC */
int sensor_init(void)
{
    LPC_SC->PCONP |= (1 << 12);                      /* ADC power */
#if ADC_CH == 4
    LPC_PINCON->PINSEL3 |= (3u << 28);               /* P1.30 = AD0.4 */
    LPC_PINCON->PINMODE3 = (LPC_PINCON->PINMODE3 & ~(3u << 28)) | (2u << 28);  /* no pull */
#elif ADC_CH == 0
    LPC_PINCON->PINSEL1 = (LPC_PINCON->PINSEL1 & ~(3u << 14)) | (1u << 14);    /* P0.23 = AD0.0 */
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~(3u << 14)) | (2u << 14);
#else
#error "set ADC_CH to 4 or 0 in config.h"
#endif
    LPC_ADC->ADCR = (1 << 21);                       /* power up the ADC */
    return 0;
}

static unsigned int sensor_read(void)
{
    unsigned int r;
    LPC_ADC->ADCR = (1 << ADC_CH) | (1 << 8) | (1 << 21) | (1 << 24);  /* clk /2, on, start */
    do { r = LPC_ADC->ADGDR; } while (!(r & 0x80000000));
    return (r >> 4) & 0xFFF;
}
#endif

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
