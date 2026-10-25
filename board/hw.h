#ifndef HW_H
#define HW_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_putint(long v);

/* Sampling: TIMER0 interrupt reads the sensor FS times a second. */
int  sensor_init(void);           /* MPU6050 WHO_AM_I value, or -1 = not found */
extern char sensor_diag[];        /* after -1: first try on each connector, e.g. "CNC A20 CNA A20" */
extern const char *sensor_where;  /* "CNC" or "CNA": where the sensor was found */
int  sensor_z(void);              /* one Z reading in milli-g, for the start-up check */
void sampler_init(void);
void sampler_arm(void);           /* wait for the next flick */
int  sampler_done(void);          /* 1 when N samples are recorded */
extern unsigned short samples[];  /* N raw sensor readings */

void lcd_init(void);
void lcd_line(int row, const char *s);   /* row 0 or 1, max 16 chars */

void keypad_init(void);
int  keypad_get(void);            /* 0..15, or -1 if no key pressed */

void leds_init(void);
void leds_set(unsigned int bits);   /* bit 0 = LD2 ... bit 7 = LD9 */

void cycles_start(void);          /* SysTick cycle counter */
unsigned long cycles_now(void);

void delay_ms(unsigned int ms);

#endif
