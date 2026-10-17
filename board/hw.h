#ifndef HW_H
#define HW_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_putint(long v);

/* Sampling: TIMER0 interrupt reads the sensor FS times a second. */
int  sensor_init(void);           /* MPU6050: WHO_AM_I value, or -1 = not found. ADXL335: 0 */
void sampler_init(void);
void sampler_arm(void);           /* wait for the next flick */
int  sampler_done(void);          /* 1 when N samples are recorded */
extern unsigned short samples[];  /* N raw sensor readings */

void lcd_init(void);
void lcd_line(int row, const char *s);   /* row 0 or 1, max 16 chars */

void keypad_init(void);
int  keypad_get(void);            /* 0..15, or -1 if no key pressed */

void delay_ms(unsigned int ms);

#endif
