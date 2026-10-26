/* Wire tester for the lab board (separate hex, not part of the detector).
   Turns the sensor pins into inputs with the LPC's weak pull-downs and shows
   them live on the LCD. A pin reads 1 only if something pulls it up: the
   GY-521's pull-up resistors (wire reaches the module) or a jumper to 3.3 V.
     C5 = CNC pin 5 (P0.19)  C6 = CNC pin 6 (P0.20)  C9 = CNC pin 9 (P2.13)
     A7 = CNA pin 7 (P0.10)  A8 = CNA pin 8 (P0.11)
   Nothing is ever driven, so touching a pin with 3.3 V is safe.          */
#include <LPC17xx.h>
#include "hw.h"

static char bit(unsigned int port, int n) { return (char)('0' + ((port >> n) & 1)); }

int main(void)
{
    char l[17] = "1  1  1  1  1  ";
    SystemInit();
    SystemCoreClockUpdate();
    lcd_init();
    LPC_PINCON->PINSEL0 &= ~((3u << 20) | (3u << 22));               /* P0.10, P0.11 GPIO */
    LPC_PINCON->PINSEL1 &= ~((3u << 6) | (3u << 8));                 /* P0.19, P0.20 GPIO */
    LPC_PINCON->PINSEL4 &= ~(3u << 26);                              /* P2.13 GPIO */
    LPC_PINCON->PINMODE0 |= (3u << 20) | (3u << 22);                 /* pull-downs */
    LPC_PINCON->PINMODE1 |= (3u << 6) | (3u << 8);
    LPC_PINCON->PINMODE4 |= (3u << 26);
    LPC_PINCON->PINMODE_OD0 &= ~((1u << 10) | (1u << 11) | (1u << 19) | (1u << 20));
    LPC_GPIO0->FIODIR &= ~((1u << 10) | (1u << 11) | (1u << 19) | (1u << 20));
    LPC_GPIO2->FIODIR &= ~(1u << 13);
    lcd_line(0, "C5 C6 C9 A7 A8");
    while (1) {
        unsigned int p0 = LPC_GPIO0->FIOPIN, p2 = LPC_GPIO2->FIOPIN;
        l[0] = bit(p0, 19); l[3] = bit(p0, 20); l[6] = bit(p2, 13);
        l[9] = bit(p0, 10); l[12] = bit(p0, 11);
        lcd_line(1, l);
        delay_ms(200);
    }
}
