/* Live sensor test (separate hex). Twice a second:
   line 1: "C5:x C6:y" = CNC pin 5 (SDA, P0.19) and pin 6 (SCL, P0.20) read
           with the LPC's weak pull-downs on. 1 = the GY-521's pull-up is
           connected, 0 = no contact.
   line 2: a slow (about 5 kHz) software-I2C read of WHO_AM_I at 0x68 ("8:")
           and 0x69 ("9:"): "8:ok=68" means the sensor answered with its ID,
           "8:-" means no ACK. Unplugging/replugging wires is safe. */
#include <LPC17xx.h>
#include "hw.h"

#define SDA (1u << 19)
#define SCL (1u << 20)

static void dly(void) { volatile int i; for (i = 0; i < 2500; i++); }   /* ~50 us */
static void lo(unsigned int b) { LPC_GPIO0->FIODIR |= b; }
static void hi(unsigned int b) { LPC_GPIO0->FIODIR &= ~b; }
static int  rd(unsigned int b) { return (LPC_GPIO0->FIOPIN & b) != 0; }

static void start(void) { hi(SDA); dly(); hi(SCL); dly(); lo(SDA); dly(); lo(SCL); dly(); }
static void stop(void)  { lo(SDA); dly(); hi(SCL); dly(); hi(SDA); dly(); }
static int send(unsigned int v)
{
    int i, ack;
    for (i = 7; i >= 0; i--) {
        if (v & (1u << i)) hi(SDA); else lo(SDA);
        dly(); hi(SCL); dly(); dly(); lo(SCL); dly();
    }
    hi(SDA); dly(); hi(SCL); dly();
    ack = !rd(SDA);
    dly(); lo(SCL); dly();
    return ack;
}
static int recv(void)                       /* one byte, then NACK */
{
    int i, b = 0;
    hi(SDA);
    for (i = 0; i < 8; i++) { dly(); hi(SCL); dly(); b = (b << 1) | rd(SDA); dly(); lo(SCL); dly(); }
    hi(SDA); dly(); hi(SCL); dly(); dly(); lo(SCL); dly();
    return b;
}
static int who(unsigned int a)              /* WHO_AM_I value, or -1 = no ACK */
{
    int ok, v = -1;
    start();
    ok = send(a << 1) && send(0x75);
    if (ok) { start(); ok = send((a << 1) | 1); }
    if (ok) v = recv();
    stop();
    return v;
}
static char hx(int v) { return (char)(v < 10 ? '0' + v : 'A' + v - 10); }
static void put(char *l, int at, int v)
{
    if (v < 0) { l[at] = '-'; l[at + 1] = ' '; l[at + 2] = ' '; l[at + 3] = ' '; l[at + 4] = ' '; }
    else { l[at] = 'o'; l[at + 1] = 'k'; l[at + 2] = '='; l[at + 3] = hx((v >> 4) & 15); l[at + 4] = hx(v & 15); }
}

int main(void)
{
    char l1[17] = "C5:x C6:x       ", l2[17] = "8:-     9:-     ";
    int i;
    SystemInit();
    SystemCoreClockUpdate();
    lcd_init();
    LPC_PINCON->PINSEL1 &= ~((3u << 6) | (3u << 8));     /* GPIO */
    LPC_PINCON->PINMODE1 |= (3u << 6) | (3u << 8);       /* weak pull-downs */
    LPC_PINCON->PINMODE_OD0 &= ~(SDA | SCL);
    LPC_GPIO0->FIOCLR = SDA | SCL;                       /* latch 0: output = drive low */
    LPC_GPIO0->FIODIR &= ~(SDA | SCL);                   /* released */
    delay_ms(100);
    for (i = 0; i < 9; i++) { lo(SCL); dly(); hi(SCL); dly(); }   /* free a stuck bus */
    stop();
    while (1) {
        l1[3] = (char)('0' + rd(SDA)); l1[8] = (char)('0' + rd(SCL));
        lcd_line(0, l1);
        put(l2, 2, who(0x68));
        put(l2, 10, who(0x69));
        lcd_line(1, l2);
        delay_ms(400);
    }
}
