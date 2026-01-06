/**
 * @file io.h
 * @brief Interfaz para entrada/salida (UART)
 * 
 * Define funciones para escribir caracteres y cadenas a la UART
 * mapeada en memoria en 0x09000000 (QEMU virt).
 */

#ifndef IO_H
#define IO_H

/**
 * @brief Escribe un caracter en la UART
 * @param c Caracter a escribir
 */
void uart_putc(char c);

/**
 * @brief Escribe una cadena terminada en '\0' en la UART
 * @param s Puntero a la cadena
 */
void uart_puts(const char *s);

/**
 * @brief Imprime con formato (similar a printf)
 * @param fmt Cadena de formato
 * @param ... Argumentos variables
 * 
 * Soporta: %c (char), %s (string), %d (int), %x (hex)
 */
void kprintf(const char *fmt, ...);

#endif // IO_H