/* Clock check (separate hex). Timer1 counts PCLK with no interrupt. The LCD
   shows the count in 1/100 s, assuming PCLK = 25 MHz, so a correct clock shows
   100 per second: 6000 after 60 s by stopwatch. A lower number means the chip
   runs slower than the firmware assumes. Restart with RESET. */
#include <LPC17xx.h>
#include "hw.h"

int main(void)
{
    char l2[17];
    unsigned long t;
    int i;
    SystemInit();
    lcd_init();
    lcd_line(0, "CLOCK CHECK     ");
    LPC_SC->PCONP |= (1 << 2);        /* TIMER1 */
    LPC_TIM1->TCR = 2;                /* reset */
    LPC_TIM1->PR = 0;
    LPC_TIM1->MCR = 0;                /* no interrupt, no reset */
    LPC_TIM1->TCR = 1;
    while (1) {
        t = LPC_TIM1->TC / ((SystemCoreClock / 4) / 100);
        for (i = 0; i < 16; i++) l2[i] = ' ';
        l2[16] = 0;
        l2[0] = 'T'; l2[1] = ':';
        for (i = 15; i >= 2; i--) { l2[i] = (char)('0' + t % 10); t /= 10; if (!t) break; }
        lcd_line(1, l2);
        delay_ms(400);
    }
}
