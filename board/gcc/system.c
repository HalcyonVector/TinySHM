/* Same clock setup as Keil's default system_LPC17xx.c: 12 MHz crystal,
   PLL0 to 400 MHz, CPU at 100 MHz, peripherals at CCLK/4. */
#include <LPC17xx.h>
uint32_t SystemCoreClock = 100000000;
void SystemInit(void)
{
    LPC_SC->SCS = 0x20;                       /* main oscillator on */
    while (!(LPC_SC->SCS & 0x40));
    LPC_SC->CCLKCFG = 3;                      /* 400 / 4 = 100 MHz */
    LPC_SC->PCLKSEL0 = 0; LPC_SC->PCLKSEL1 = 0;
    LPC_SC->CLKSRCSEL = 1;
    LPC_SC->PLL0CFG = (99) | (5 << 16);       /* M=100, N=6 -> 400 MHz */
    LPC_SC->PLL0FEED = 0xAA; LPC_SC->PLL0FEED = 0x55;
    LPC_SC->PLL0CON = 1;
    LPC_SC->PLL0FEED = 0xAA; LPC_SC->PLL0FEED = 0x55;
    while (!(LPC_SC->PLL0STAT & (1 << 26)));
    LPC_SC->PLL0CON = 3;
    LPC_SC->PLL0FEED = 0xAA; LPC_SC->PLL0FEED = 0x55;
    while (!(LPC_SC->PLL0STAT & ((1 << 25) | (1 << 24))));
    LPC_SC->FLASHCFG = 0x403A;
    LPC_SC->PCONP |= (1 << 15);               /* GPIO */
}
void SystemCoreClockUpdate(void) { SystemCoreClock = 100000000; }
