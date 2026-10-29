/* Clock check (separate hex). Timer0 is set to tick 100 times per second
   assuming the chip runs at 100 MHz. The LCD shows the tick count since reset.
   Start a stopwatch on a reset (RESET button) and compare: with a correct clock
   the count is 100 per second, so 6000 after 60 s. A lower count means the
   chip runs slower than the firmware assumes. */
#include <LPC17xx.h>
#include "hw.h"

static volatile unsigned long ticks = 0;

void TIMER1_IRQHandler(void)
{
    LPC_TIM1->IR = 1;                 /* clear MR0 flag */
    ticks++;
}

int main(void)
{
    char l2[17];
    unsigned long t;
    SystemInit();
    SystemCoreClockUpdate();
    lcd_init();
    LPC_SC->PCONP |= (1 << 2);        /* TIMER1 */
    LPC_TIM1->TCR = 2;                /* reset */
    LPC_TIM1->PR = 0;
    LPC_TIM1->MR0 = (SystemCoreClock / 4) / 100 - 1;   /* 100 Hz */
    LPC_TIM1->MCR = 3;                /* interrupt + reset on MR0 */
    NVIC_EnableIRQ(TIMER1_IRQn);
    LPC_TIM1->TCR = 1;
    while (1) {
        t = ticks;
        lcd_line(0, "CLOCK CHECK     ");
        /* show the count as decimal digits, right aligned */
        {
            int i;
            for (i = 0; i < 16; i++) l2[i] = ' ';
            l2[16] = 0;
            for (i = 15; i >= 8; i--) { l2[i] = (char)('0' + t % 10); t /= 10; if (!t) break; }
            l2[0] = 'T'; l2[1] = ':';
        }
        lcd_line(1, l2);
        delay_ms(400);
    }
}
