#include <stdint.h>
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);
extern void SystemInit(void);
void Reset_Handler(void)
{
    uint32_t *s = &_sidata, *d = &_sdata;
    while (d < &_edata) *d++ = *s++;
    for (d = &_sbss; d < &_ebss;) *d++ = 0;
    main();
    while (1);
}
void Default_Handler(void) { while (1); }
void TIMER0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMER1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)  __attribute__((weak, alias("Default_Handler")));
__attribute__((section(".isr_vector"), used))
void (*const vectors[16 + 35])(void) = {
    (void (*)(void))&_estack, Reset_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    0, 0, 0, 0,
    Default_Handler, Default_Handler, 0, Default_Handler, SysTick_Handler,
    Default_Handler, TIMER0_IRQHandler,   /* WDT=16, TIMER0=17 */
    TIMER1_IRQHandler,                    /* TIMER1=18 */
    [18 ... 50] = Default_Handler,
};
