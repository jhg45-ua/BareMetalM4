/**
 * @file io.c
 * @brief Implementacion de entrada/salida y funciones de impresion
 * 
 * @details
 *   Implementa:
 *   - UART de QEMU virt (simple, sin control de flujo)
 *   - Funciones basicas uart_putc() y uart_puts()
 *   - kprintf() con soporte para %c, %s, %d, %x
 * 
 *   LA UART EN QEMU:
 *   Direccion: 0x09000000 (definido en link.ld)
 *   Escribir en esta direccion envia caracteres a la consola.
 *   Es una UART muy simplificada, sin:
 *   - Status registers (no verificamos si esta ocupada)
 *   - FIFO control (asumimos que siempre acepta datos)
 *   - Interrupciones (polling en lugar de event-driven)
 * 
 * @author Sistema Operativo Educativo
 * @version 0.3
 * @see io.h para interfaz publica
 */

#include <stdarg.h>
#include "../include/io.h"

/* Registro base de la UART en QEMU virt (0x09000000) */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;

/* Escribe un caracter en la UART */
void uart_putc(char c) { 
    *UART0_DIR = (unsigned int)c;
}

/* Escribe una cadena nula-terminada en la UART */
void uart_puts(const char *s) { 
    while (*s) uart_putc(*s++); 
}

/* Convierte un numero a cadena y lo imprime */
void print_num(long val, int base) {
    char buf[32];
    int i = 0;
    int sign = 0;

    /* Manejo de numeros negativos (solo para base 10) */
    if (base == 10 && val < 0) {
        sign = 1;
        val = -val;
    }

    /* Caso especial: 0 */
    if (val == 0) {
        uart_putc('0');
        return;
    }

    /* Division repetida para extraer digitos */
    unsigned long uval = (unsigned long)val;

    while (uval > 0) {
        int digit = uval % base;
        buf[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
        uval /= base;
    }

    /* Imprimir signo si es negativo */
    if (sign) uart_putc('-');

    /* Imprimir buffer al reves */
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

/* Imprime con formato tipo printf (kernel printf) */
void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            // Manejo de especificadores de formato
            switch (*fmt) {
                case 'c': {
                    int c = va_arg(args, int);
                    uart_putc((char)c);
                    break;
                }
                
                // Caso de cadena de caracteres
                case 's': {
                    char *s = va_arg(args, char *);
                    uart_puts(s);
                    break;
                }
                
                // Caso de numero decimal
                case 'd': {
                    long d = va_arg(args, long);
                    print_num(d, 10);
                    break;
                }
                
                // Caso de numero hexadecimal
                case 'x': {
                    unsigned long x = va_arg(args, unsigned long);
                    print_num((long)x, 16);
                    break;
                }
                
                // Caso por defecto: imprimir literalmente
                default:
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