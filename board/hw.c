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
/* MPU6050 over I2C. It can sit on either of two connectors, and the board
   finds it at power-up:
   - CNC pin 5 = P0.19 (SDA1), pin 6 = P0.20 (SCL1), with the I2C1 peripheral
   - CNA pin 7 = P0.10 (SDA2), pin 8 = P0.11 (SCL2), with the I2C2 peripheral
     (then the LED cable is unplugged: the sensor takes its socket)
   On each connector it first tries the I2C peripheral, then software
   ("bit-banged") I2C on the same two pins. Every wait has a timeout, so
   bad wiring shows up as "not found" instead of a frozen board. */
#define PORT_CNC 0
#define PORT_CNA 1

static int port;                                /* PORT_CNC or PORT_CNA */
static LPC_I2C_TypeDef *i2c;                    /* LPC_I2C1 or LPC_I2C2 */
static unsigned int sda_bit, scl_bit;           /* GPIO0 bits of the two pins */
static int use_hw;                              /* 1 = I2C peripheral, 0 = software */
static unsigned int i2c_loops;                  /* busy-loop count for a quarter bit */
unsigned int led_mask = 0xFF;                   /* LED pins still driven (sensor on CNA: P0.10, P0.11 are not) */

/* --- software I2C. A line is driven low by making the pin an output (its
   output latch is 0) and released by making it an input, so the GY-521's
   4.7k pull-ups pull it high. */
static void i2c_delay(void)
{
    volatile unsigned int i;
    for (i = 0; i < i2c_loops; i++);
}

static void sda_low(void)  { LPC_GPIO0->FIODIR |= sda_bit; }
static void sda_high(void) { LPC_GPIO0->FIODIR &= ~sda_bit; }
static void scl_low(void)  { LPC_GPIO0->FIODIR |= scl_bit; }
static int  sda_read(void) { return (LPC_GPIO0->FIOPIN & sda_bit) != 0; }

static int scl_high(void)                       /* release SCL, wait if a chip holds it low */
{
    long t = 10000;
    LPC_GPIO0->FIODIR &= ~scl_bit;
    while (!(LPC_GPIO0->FIOPIN & scl_bit))
        if (--t == 0) return 0;
    return 1;
}

static void sw_start(void)                      /* also works as a repeated start */
{
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    sda_low();  i2c_delay();
    scl_low();  i2c_delay();
}

static void sw_stop(void)
{
    sda_low();  i2c_delay();
    scl_high(); i2c_delay();
    sda_high(); i2c_delay();
}

static int sw_send(unsigned int b)              /* 1 = the chip acknowledged */
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

static int sw_recv(int ack)
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

static void sw_recover(void)
{
    /* If the board was reset in the middle of a read, a chip may still be
       holding SDA low. Nine clock pulses let it finish, then a STOP. */
    int i;
    sda_high();
    for (i = 0; i < 9; i++) {
        scl_low(); i2c_delay(); i2c_delay();
        scl_high(); i2c_delay(); i2c_delay();
    }
    sw_start();
    sw_stop();
}

/* --- I2C peripheral (UM10360 chapter 19 state codes) */
#define I2C_AA   0x04
#define I2C_SI   0x08
#define I2C_STO  0x10
#define I2C_STA  0x20
#define I2C_EN   0x40

static int hw_wait(void)
{
    long t = 100000;
    while (!(i2c->I2CONSET & I2C_SI))
        if (--t == 0) return -1;
    return (int)i2c->I2STAT;
}

static int hw_start(void)                       /* 0x08 start, 0x10 repeated start */
{
    int st;
    i2c->I2CONSET = I2C_STA;
    i2c->I2CONCLR = I2C_SI;
    st = hw_wait();
    i2c->I2CONCLR = I2C_STA;
    return st;
}

static int hw_send(unsigned int b)              /* 0x18 / 0x28 / 0x40 = acked */
{
    i2c->I2DAT = b & 0xFF;
    i2c->I2CONCLR = I2C_SI;
    return hw_wait();
}

static int hw_recv(int ack)
{
    if (ack) i2c->I2CONSET = I2C_AA;
    else     i2c->I2CONCLR = I2C_AA;
    i2c->I2CONCLR = I2C_SI;
    if (hw_wait() < 0) return -1;
    return (int)(i2c->I2DAT & 0xFF);
}

static void hw_stop(void)
{
    long t = 100000;
    i2c->I2CONSET = I2C_STO;
    i2c->I2CONCLR = I2C_SI;
    while ((i2c->I2CONSET & I2C_STO) && --t);
}

/* Pin setup for one connector. Always: no internal pull (the GY-521 has
   4.7k pull-ups), open drain, output latch 0, pins released. */
static void pins_select(int p, int peripheral)
{
    port = p;
    if (p == PORT_CNC) {
        i2c = LPC_I2C1; sda_bit = 1u << 19; scl_bit = 1u << 20;
        LPC_SC->PCONP |= (1 << 19);                              /* I2C1 power */
        LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~((3u << 6) | (3u << 8))) | (2u << 6) | (2u << 8);
        LPC_PINCON->PINSEL1 &= ~((3u << 6) | (3u << 8));
        if (peripheral) LPC_PINCON->PINSEL1 |= (3u << 6) | (3u << 8);   /* SDA1, SCL1 */
    } else {
        i2c = LPC_I2C2; sda_bit = 1u << 10; scl_bit = 1u << 11;
        LPC_SC->PCONP |= (1 << 26);                              /* I2C2 power */
        LPC_PINCON->PINMODE0 = (LPC_PINCON->PINMODE0 & ~((3u << 20) | (3u << 22))) | (2u << 20) | (2u << 22);
        LPC_PINCON->PINSEL0 &= ~((3u << 20) | (3u << 22));
        if (peripheral) LPC_PINCON->PINSEL0 |= (2u << 20) | (2u << 22);  /* SDA2, SCL2 */
    }
    LPC_PINCON->PINMODE_OD0 |= sda_bit | scl_bit;
    LPC_GPIO0->FIOCLR = sda_bit | scl_bit;
    LPC_GPIO0->FIODIR &= ~(sda_bit | scl_bit);
    if (peripheral) {
        unsigned int half = (SystemCoreClock / 4) / (2 * I2C_HZ);   /* PCLK = CCLK/4 */
        i2c->I2CONCLR = I2C_AA | I2C_SI | I2C_STA | I2C_EN;
        i2c->I2SCLH = half;
        i2c->I2SCLL = half;
        i2c->I2CONSET = I2C_EN;
    } else {
        i2c->I2CONCLR = I2C_EN;                     /* peripheral off, pins are GPIO */
    }
    use_hw = peripheral;
}

/* --- MPU6050 register access */
static unsigned int mpu_addr = MPU_ADDR;        /* 0x68, or 0x69 if AD0 is high */
char sensor_diag[17];                           /* LCD line 2 when the sensor is not found */
const char *sensor_where = "CNC";               /* connector the sensor was found on */

static int mpu_write(unsigned int reg, unsigned int val)
{
    int ok;
    if (use_hw) {
        ok = hw_start() == 0x08 && hw_send(mpu_addr << 1) == 0x18 &&
             hw_send(reg) == 0x28 && hw_send(val) == 0x28;
        hw_stop();
    } else {
        sw_start();
        ok = sw_send(mpu_addr << 1) && sw_send(reg) && sw_send(val);
        sw_stop();
    }
    return ok;
}

static int mpu_read(unsigned int reg, unsigned char *buf, int n)
{
    int i, v, ok;
    if (use_hw) {
        ok = hw_start() == 0x08 && hw_send(mpu_addr << 1) == 0x18 &&
             hw_send(reg) == 0x28 && hw_start() == 0x10 &&
             hw_send((mpu_addr << 1) | 1) == 0x40;
    } else {
        sw_start();
        ok = sw_send(mpu_addr << 1) && sw_send(reg);
        if (ok) { sw_start(); ok = sw_send((mpu_addr << 1) | 1); }
    }
    for (i = 0; ok && i < n; i++) {
        v = use_hw ? hw_recv(i < n - 1) : sw_recv(i < n - 1);
        if (v < 0) ok = 0; else buf[i] = (unsigned char)v;
    }
    if (use_hw) hw_stop(); else sw_stop();
    return ok;
}

static char hex_digit(int v) { return (char)(v < 10 ? '0' + v : 'A' + v - 10); }

/* One WHO_AM_I read with the I2C peripheral, step by step. Returns the
   value read, or -1. d gets 3 characters for the LCD: the step that went
   wrong ("S" start, "A" address, "R" register, "P" repeated start, "D"
   read address, "V" value) and the I2C status in hex ("TO" = timeout). */
static int hw_try(unsigned int addr, char *d)
{
    int st, id = -1;
    char step = 'S';
    st = hw_start();
    if (st == 0x08) { step = 'A'; st = hw_send(addr << 1); }
    if (st == 0x18) { step = 'R'; st = hw_send(0x75); }
    if (st == 0x28) { step = 'P'; st = hw_start(); }
    if (st == 0x10) { step = 'D'; st = hw_send((addr << 1) | 1); }
    if (st == 0x40) {                            /* read one byte, NACK it: 0x58 = got it */
        step = 'V';
        i2c->I2CONCLR = I2C_AA;
        i2c->I2CONCLR = I2C_SI;
        st = hw_wait();
        if (st == 0x58) st = id = (int)(i2c->I2DAT & 0xFF);
    }
    hw_stop();
    d[0] = step;
    if (st < 0) { d[1] = 'T'; d[2] = 'O'; }
    else        { d[1] = hex_digit((st >> 4) & 0xF); d[2] = hex_digit(st & 0xF); }
    return id;
}

/* Look for the sensor on one connector: 10 tries with the peripheral
   (0x68 and 0x69 in turn), then 6 with software I2C. Returns WHO_AM_I or
   -1; d gets the result of the first peripheral try. */
static int probe_port(int p, char *d)
{
    int k, v;
    unsigned char id;

    pins_select(p, 1);
    delay_ms(5);
    for (k = 0; k < 10; k++) {
        mpu_addr = (k & 1) ? MPU_ADDR + 1 : MPU_ADDR;
        v = hw_try(mpu_addr, k == 0 ? d : d + 4);
        if (v >= 0) return v;                    /* a chip answered WHO_AM_I */
        delay_ms(10);
    }

    pins_select(p, 0);
    sw_recover();
    for (k = 0; k < 6; k++) {
        mpu_addr = (k & 1) ? MPU_ADDR + 1 : MPU_ADDR;
        if (mpu_read(0x75, &id, 1)) return id;
        delay_ms(5);
    }
    mpu_addr = MPU_ADDR;
    return -1;
}

int sensor_init(void)
{
    int v;
    char cnc[8], cna[8];

    i2c_loops = SystemCoreClock / (4 * 4 * I2C_HZ);  /* ~4 cycles per loop, 4 quarters per bit */
    if (i2c_loops == 0) i2c_loops = 1;
    delay_ms(100);                                   /* MPU6050 power-up time */

    v = probe_port(PORT_CNC, cnc);
    if (v >= 0) { sensor_where = "CNC"; goto found; }

    /* Not on CNC: try CNA pins 7 and 8. The LEDs on P0.10, P0.11 are
       given up for this. */
    led_mask = 0x3F;
    v = probe_port(PORT_CNA, cna);
    if (v >= 0) { sensor_where = "CNA"; goto found; }

    /* Not found. LCD line 2, for example "CNC A20 CNA A20":
       the first peripheral try on each connector (see hw_try). */
    led_mask = 0xFF;
    pins_select(PORT_CNA, 0);                        /* CNA pins back to plain GPIO */
    LPC_PINCON->PINMODE_OD0 &= ~((1u << 10) | (1u << 11));
#if USE_LEDS
    LPC_GPIO0->FIODIR |= (0xFFu << 4);               /* and back to the LEDs */
#endif
    sensor_diag[0] = 'C'; sensor_diag[1] = 'N'; sensor_diag[2] = 'C'; sensor_diag[3] = ' ';
    sensor_diag[4] = cnc[0]; sensor_diag[5] = cnc[1]; sensor_diag[6] = cnc[2];
    sensor_diag[7] = ' ';
    sensor_diag[8] = 'C'; sensor_diag[9] = 'N'; sensor_diag[10] = 'A'; sensor_diag[11] = ' ';
    sensor_diag[12] = cna[0]; sensor_diag[13] = cna[1]; sensor_diag[14] = cna[2];
    sensor_diag[15] = 0;
    return -1;

found:
    mpu_write(0x6B, 0x01);                           /* wake up, clock from gyro X PLL */
    delay_ms(10);
    mpu_write(0x1A, 0x03);                           /* low-pass filter 44 Hz (anti-alias for FS = 100) */
    mpu_write(0x19, 1000 / FS - 1);                  /* sample rate 1 kHz / 10 = 100 Hz */
    mpu_write(0x1C, 0x08);                           /* accelerometer range +-4 g */
    return v;
}

int sensor_z(void)                                   /* one Z reading in milli-g, for the start-up check */
{
    unsigned char b[2];
    if (!mpu_read(0x3F, b, 2)) return -99999;
    return (int)((short)((b[0] << 8) | b[1])) * 1000 / (int)COUNTS_PER_G;
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
    LPC_GPIO0->FIOCLR = (led_mask << 4);
    LPC_GPIO0->FIOSET = (bits & led_mask) << 4;
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
