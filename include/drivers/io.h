/**
 * @file io.h
 * @brief Interfaz para entrada/salida (UART)
 * 
 * @details
 *   Define funciones para escribir caracteres y cadenas a la UART
 *   mapeada en memoria en 0x09000000 (QEMU virt).
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
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
 * @brief Inicializa las interrupciones UART
 */
void uart_irq_init();

/**
 * @brief Manejador de interrupciones UART
 */
void uart_handle_irq();

/**
 * @brief Lee un carácter sin bloqueo
 * @return Carácter leído o 0 si no hay datos
 */
char uart_getc_nonblocking();

/**
 * @brief Imprime con formato (similar a printf)
 * @param fmt Cadena de formato
 * @param ... Argumentos variables
 * 
 * Soporta: %c (char), %s (string), %d (int), %x (hex)
 */
void kprintf(const char *fmt, ...);

#endif // IO_H