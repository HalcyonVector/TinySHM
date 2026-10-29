/* LCD-only test (separate hex). No timer and no interrupts: shows a fixed
   line 1 and a counter on line 2 that goes up about once per second.
   If line 1 shows in full, the clock check hex's timer code is the problem. */
#include <LPC17xx.h>
#include "hw.h"

int main(void)
{
    char l2[17];
    unsigned long n = 0;
    int i;
    SystemInit();
    lcd_init();
    lcd_line(0, "LCD TEST OK     ");
    while (1) {
        for (i = 0; i < 16; i++) l2[i] = ' ';
        l2[16] = 0;
        {
            unsigned long t = n;
            for (i = 15; i >= 0; i--) { l2[i] = (char)('0' + t % 10); t /= 10; if (!t) break; }
        }
        lcd_line(1, l2);
        delay_ms(1000);
        n++;
    }
}
