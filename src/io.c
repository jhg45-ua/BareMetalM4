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
#include "../include/types.h"
#include "../include/io.h"

/* Registro base de la UART en QEMU virt (0x09000000) */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;

/* Direcciones del UART PL011 */
volatile unsigned int * const UART0_DR   = (unsigned int *)0x09000000; /* Data Register */
volatile unsigned int * const UART0_FR   = (unsigned int *)0x09000018; /* Flag Register */
volatile unsigned int * const UART0_IMSC = (unsigned int *)0x09000038; /* Interrupt Mask Set/Clear */
volatile unsigned int * const UART0_ICR  = (unsigned int *)0x09000044; /* Interrupt Clear Register */

/* Escribe un caracter en la UART */
void uart_putc(char c) { 
    *UART0_DIR = (unsigned int)c;
}

/* Escribe una cadena nula-terminada en la UART */
void uart_puts(const char *s) { 
    while (*s) uart_putc(*s++); 
}

/* --- Buffer Circular de teclado */
#define KB_BUFFER_SIZE 128

volatile char kb_buffer[KB_BUFFER_SIZE];
volatile int kb_head = 0; // Donde escribe la interrupcion
volatile int kb_tail = 0; // Donde lee el Shell

/* --- Inicializar interrupciones UART --- */
void uart_irq_init() {
    /* Activamos el bit 4 (RXIM) para que interrumpa al recibir datos */
    *UART0_IMSC = (1 << 4);
}

/* --- Manejador de la interrupcion UART --- */
/* Esta funcion la llamara el GIC cuando detecte ID 33 */
void uart_handle_irq() {
    /* Bucle: Leemos mientras el bit 4 de Flags (RXFE - RX FIFO Empty) sea 0.
       Es decir: "Mientras NO esté vacío, lee". */
    
    while ( !(*UART0_FR & (1 << 4)) ) {
        char c = (char)(*UART0_DR);

        /* Guardar en buffer circular */
        int next = (kb_head + 1) % KB_BUFFER_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = c;
            kb_head = next;
        }
    }

    /* Limpiar interrupciones: Recepción (bit 4) y Timeout (bit 6) */
    *UART0_ICR = (1 << 4) | (1 << 6); 
}

/* --- Funcion para que el Shell lea una tecla */
/* Devuelve 0 si no hay teclas, o el caracter si lo hay */
char uart_getc_nonblocking() {
    if (kb_head == kb_tail) return 0; // Buffer vacio

    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
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