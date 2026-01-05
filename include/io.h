/**
 * @file io.h
 * @brief Interfaz para entrada/salida (UART) y funciones de impresion
 * 
 * @details
 *   Define las funciones para:
 *   - Escribir caracteres a la UART (uart_putc)
 *   - Escribir cadenas (uart_puts)
 *   - Imprimir con formato tipo printf (kprintf)
 * 
 *   La UART esta mapeada en memoria en 0x09000000 (QEMU virt).
 *   Escribir en esta direccion envia datos a la consola.
 * 
 * @see io.c para implementacion
 */

#ifndef IO_H
#define IO_H

/* ========================================================================== */
/* FUNCIONES DE SALIDA (UART)                                               */
/* ========================================================================== */

/**
 * @brief Escribe un caracter en la UART
 * 
 * @param c Caracter ASCII a escribir
 * 
 * @details
 *   Escribe un caracter individual en el registro UART0_DIR (0x09000000).
 *   La UART en QEMU virt imprime el caracter en la terminal de salida.
 * 
 *   FUNCIONAMIENTO:
 *   1. Escribe el caracter en la direccion MMIO 0x09000000
 *   2. La UART recibe el dato
 *   3. El caracter aparece en la consola
 * 
 *   NOTA: Esta es una UART simplificada (polling).
 *   Un driver real verificaria flags de estado (TX FIFO lleno, etc).
 * 
 * @see uart_puts() para escribir cadenas
 * @see kprintf() para impresion con formato
 * 
 * @example
 * @code
 * uart_putc('H');
 * uart_putc('i');
 * // Salida en consola: "Hi"
 * @endcode
 */
void uart_putc(char c);

/**
 * @brief Escribe una cadena nula-terminada en la UART
 * 
 * @param s Puntero a cadena de caracteres (terminada con '\0')
 * 
 * @details
 *   Itera sobre la cadena llamando uart_putc() para cada caracter.
 *   Se detiene cuando encuentra el caracter nulo terminador ('\0').
 * 
 *   ALGORITMO:
 *   @code
 *   mientras s[i] != '\0':
 *       uart_putc(s[i])
 *       i++
 *   @endcode
 * 
 * @warning Si 's' no es una cadena valida (no nula-terminada),
 *          continuara escribiendo en memoria mas alla, causando
 *          comportamiento indefinido (crash probable).
 * 
 * @see uart_putc() para caracteres individuales
 * @see kprintf() para impresion formateada
 * 
 * @example
 * @code
 * uart_puts("Hola SO!");
 * // Salida en consola: "Hola SO!"
 * @endcode
 */
void uart_puts(const char *s);

/**
 * @brief Imprime con formato tipo printf (kernel printf)
 * 
 * @param fmt Cadena de formato con especificadores %c, %s, %d, %x
 * @param ... Argumentos variables (segun formato)
 * 
 * @details
 *   Funcion similar a printf() pero para el kernel.
 *   Soporta los siguientes especificadores:
 * 
 *   - %c: Caracter (char)
 *   - %s: Cadena (const char *)
 *   - %d: Entero decimal con signo (int/long)
 *   - %x: Entero hexadecimal sin signo (long)
 *   
 *   PROCESAMIENTO:
 *   1. Itera sobre la cadena de formato
 *   2. Si encuentra '%', lee el siguiente caracter
 *   3. Extrae el argumento correspondiente (va_arg)
 *   4. Convierte a cadena segun el formato
 *   5. Escribe en UART
 *   
 *   LIMITACIONES:
 *   - No soporta ancho de campo (%5d), precision (%.2f), etc
 *   - Los especificadores invalidos simplemente se escriben literalmente
 *   - Sin buffer interno (escritura directa a UART)
 * 
 * @note Usa varargs de <stdarg.h> para argumentos variables
 * 
 * @see uart_puts() para cadenas sin formato
 * @see uart_putc() para caracteres
 * 
 * @example
 * @code
 * int pid = 5;
 * kprintf("Proceso %d iniciado\n", pid);
 * kprintf("Direccion: 0x%x\n", 0x80000000);
 * kprintf("Mensaje: %s\n", "Hola");
 * 
 * // Salida:
 * // Proceso 5 iniciado
 * // Direccion: 0x80000000
 * // Mensaje: Hola
 * @endcode
 * 
 * @note Se usa ampliamente en debug y logs del kernel
 */
void kprintf(const char *fmt, ...);

#endif // IO_H