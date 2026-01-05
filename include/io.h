/* src/io.h */
#ifndef IO_H
#define IO_H

void uart_putc(char c);
void uart_puts(const char *);
void kprintf(const char *fmt, ...);

#endif // IO_H