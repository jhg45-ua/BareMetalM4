/* src/io.c */
#include <stdarg.h>
#include "io.h"

/* Direccion del UART en QEMU virt */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;

/* Funcion auxiliar para escribir un caracter  */
void uart_putc(char c) { 
    *UART0_DIR = c; /* Escribir en esta direccion envia el dato a la terminal */ 
}

/* Funcion auxiliar para escribir una cadena */
void uart_puts(const char *s) { 
    while (*s) uart_putc(*s++); 
}

/* Funcion para imprimir numeros en distintas bases */
void print_num(long val, int base) {
    char buf[32];
    int i = 0;
    int sign = 0;


    /* Manejo numero de negativos solo para base 10 */
    if (base == 10 && val < 0) {
        sign = 1;
        val = -val;
    }

    /* Caso especial para 0 */
    if (val == 0) {
        uart_putc('0');
        return;
    }

    /* Conversion de numero a cadena (en orden inverso) */
    /* Usamos unsigned long para las operaciones para evitar problemas con el signo */
    unsigned long uval = (unsigned long)val;

    while (uval > 0) {
        int digit = uval % base;
        buf[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
        uval /= base;
    }

    if (sign) uart_putc('-');

    /* Imprimir el buffer al reves */
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}


void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++; /* Saltar el '%' */
            switch (*fmt) {
                case 'c': {
                    /* char se promociona a int en argumentos variables */
                    int c = va_arg(args, int);
                    uart_putc(c);
                    break;
                }
                case 's': {
                    char *s = va_arg(args, char *);
                    uart_puts(s);
                    break;
                }
                case 'd': {
                    long d = va_arg(args, long);
                    print_num(d, 10);
                    break;
                }
                case 'x': {
                    unsigned long x = va_arg(args, unsigned long);
                    print_num(x, 16);
                    break;
                }
                default: /* Si no conocemos el formato, improvisamos el % y la letra */
                    uart_putc('%');
                    uart_putc(*fmt);
                    break;
            }
        } else {
            uart_putc(*fmt);
        }
        fmt++;
    }
    va_end(args);
}