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

/* ========================================================================== */
/* CONFIGURACION DEL HARDWARE (UART)                                        */
/* ========================================================================== */

/**
 * @brief Registro base de la UART en QEMU virt
 * 
 * @details
 *   Direccion fisica: 0x09000000 (Memory-Mapped I/O)
 *   Escribir un byte en esta direccion envia a la consola serial.
 *   
 *   TIPO volatile:
 *   - volatile: Le dice al compilador que no cachee el valor
 *   - const: La direccion nunca cambia
 *   - unsigned int *: El registro es de 32 bits
 *   
 *   NOTA: Aunque es de 32 bits, solo usamos el byte bajo [7:0]
 *         Los bits altos [31:8] son ignorados por la UART.
 */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;

/* ========================================================================== */
/* FUNCIONES DE SALIDA                                                       */
/* ========================================================================== */

/**
 * @brief Escribe un caracter en la UART
 * 
 * @param c Caracter ASCII a escribir
 * 
 * @details
 *   OPERACION BASICA:
 *   1. Escribe el caracter en la direccion UART0_DIR
 *   2. La UART envia a la consola
 *   3. Retorna inmediatamente (sin esperar confirmacion)
 * 
 *   LIMITACIONES:
 *   - Sin verificacion de estado (busy, FIFO lleno, etc)
 *   - Sin handshake (asumimos que siempre acepta)
 *   - Posible perdida de datos si escribimos muy rapido
 * 
 *   Un driver real haria:
 *   @code
 *   void uart_putc(char c) {
 *       while (!uart_ready());  // Esperar a que UART este lista
 *       *UART0_DIR = c;         // Escribir
 *   }
 *   @endcode
 * 
 *   CASTING:
 *   - (unsigned int)c: Convierte char a 32 bits (extiende ceros)
 *   - Solo el byte bajo se envia, el resto se ignora
 */
void uart_putc(char c) { 
    *UART0_DIR = (unsigned int)c;
}

/**
 * @brief Escribe una cadena nula-terminada en la UART
 * 
 * @param s Puntero a cadena (debe terminar con '\0')
 * 
 * @details
 *   ALGORITMO:
 *   @code
 *   mientras (*s != '\0') {
 *       uart_putc(*s);  // Escribir caracter actual
 *       s++;            // Avanzar al siguiente
 *   }
 *   @endcode
 * 
 *   BUCLE SIMPLE:
 *   while (*s) uart_putc(*s++);
 *   
 *   Nota: El post-incremento (s++) devuelve el valor anterior,
 *   asi que antes de avanzar, usamos el puntero actual.
 */
void uart_puts(const char *s) { 
    while (*s) uart_putc(*s++); 
}

/* ========================================================================== */
/* CONVERSION DE NUMEROS                                                      */
/* ========================================================================== */

/**
 * @brief Convierte un numero a cadena y lo imprime
 * 
 * @param val Valor numerico a imprimir
 * @param base Base numerica (10=decimal, 16=hexadecimal, etc)
 * 
 * @details
 *   ALGORITMO DIVISION REPETIDA:
 *   1. Extraer el digito menos significativo (val % base)
 *   2. Convertir a caracter ('0'-'9', 'A'-'F')
 *   3. Guardar en buffer
 *   4. Descartar el digito (val /= base)
 *   5. Repetir hasta val = 0
 *   6. Imprimir buffer al reves (fue generado invertido)
 * 
 *   EJEMPLO PARA DECIMAL (123, base 10):
 *   @code
 *   123 % 10 = 3 -> buf[0] = '3' -> 123 / 10 = 12
 *   12 % 10 = 2  -> buf[1] = '2' -> 12 / 10 = 1
 *   1 % 10 = 1   -> buf[2] = '1' -> 1 / 10 = 0
 *   
 *   Buffer: "321"
 *   Imprimir al reves: "123"
 *   @endcode
 * 
 *   MANEJO DE SIGNO:
 *   - Solo para base 10 (otras son unsigned)
 *   - Si val < 0: marca sign=1, convierte a positivo
 *   - Imprime '-' antes del numero
 * 
 *   CASO ESPECIAL: val=0
 *   El bucle nunca entraria (0 es falso)
 *   Manejamos explicitamente: imprimir '0' y retornar
 * 
 *   BASES SOPORTADAS:
 *   - 10: Decimal "0"-"9"
 *   - 16: Hexadecimal "0"-"9", "A"-"F"
 *   - Otras: Funcionan pero poco frecuentes
 * 
 * @note unsigned long: Evita problemas con signo en divisiones
 * @see kprintf() que llama esta funcion para %d y %x
 */
void print_num(long val, int base) {
    char buf[32];
    int i = 0;
    int sign = 0;

    /**
     * Manejo de numeros negativos (solo para base 10)
     */
    if (base == 10 && val < 0) {
        sign = 1;
        val = -val;
    }

    /**
     * Caso especial: 0
     * El bucle no entraria, asi que lo tratamos aqui
     */
    if (val == 0) {
        uart_putc('0');
        return;
    }

    /**
     * Division repetida para extraer digitos
     * 
     * unsigned long uval: Evita problemas con operaciones en negativos
     * Aunque val ya fue convertido a positivo, usamos unsigned para seguridad
     */
    unsigned long uval = (unsigned long)val;

    while (uval > 0) {
        int digit = uval % base;
        /**
         * Conversion a caracter:
         * - digit 0-9 -> '0'-'9'
         * - digit 10-15 -> 'A'-'F'
         * 
         * (digit < 10) ? (digit + '0') : (digit - 10 + 'A')
         * La segunda parte: digit=10 -> 10-10+'A'=0+'A'='A'
         */
        buf[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
        uval /= base;
    }

    /**
     * Imprimir signo si es negativo
     */
    if (sign) uart_putc('-');

    /**
     * Imprimir buffer al reves
     * Fue generado invertido (LSD primero), asi que iteramos hacia atras
     */
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

/* ========================================================================== */
/* IMPRESION CON FORMATO                                                     */
/* ========================================================================== */

/**
 * @brief Imprime con formato tipo printf (kernel printf)
 * 
 * @param fmt Cadena de formato con especificadores
 * @param ... Argumentos variables
 * 
 * @details
 *   IMPLEMENTACION:
 *   1. Iterar sobre cadena de formato
 *   2. Si caracter es '%': leer siguiente, es especificador
 *   3. Extraer argumento con va_arg() segun tipo
 *   4. Convertir a cadena y escribir
 *   5. Si caracter no es '%': escribir directamente
 * 
 *   ESPECIFICADORES SOPORTADOS:
 *   - %c: Caracter (char)
 *   - %s: Cadena (const char *)
 *   - %d: Entero decimal (int/long)
 *   - %x: Hexadecimal sin signo (long)
 *   - %%: Caracter '%' literal
 *   
 *   ESPECIFICADORES NO SOPORTADOS:
 *   - %f: Float (no implementado)
 *   - %5d: Ancho de campo (no implementado)
 *   - %.2f: Precision (no implementado)
 *   - %o: Octal (no implementado)
 *   
 *   SECUENCIA DE VARARGS:
 *   @code
 *   va_list args;
 *   va_start(args, fmt);     // Inicializar iterator
 *   long val = va_arg(args, long);  // Extraer proximo argumento
 *   va_end(args);            // Finalizar
 *   @endcode
 * 
 *   EJEMPLO DE FUNCIONAMIENTO:
 *   @code
 *   kprintf("Proceso %d iniciado en 0x%x\\n", 5, 0x80000);
 *   
 *   Iteracion:
 *   'P' -> uart_putc('P')
 *   'r', 'o', 'c', ... -> uart_putc(...)
 *   '%' -> especificador siguiente
 *   'd' -> extraer int, convertir decimal "5", uart_puts("5")
 *   ...
 *   '%' -> especificador siguiente
 *   'x' -> extraer long, convertir hex "80000", uart_puts("80000")
 *   '\\n' -> uart_putc('\\n')
 *   @endcode
 * 
 * @note va_arg(args, type): El type debe coincidir con lo pasado
 *       Si mismatch: comportamiento indefinido (crash probable)
 * 
 * @see uart_putc() para caracteres
 * @see uart_puts() para cadenas
 * @see print_num() para conversion numerica
 */
void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;  /**< Saltar el '%', leer especificador */
            
            switch (*fmt) {
                /**
                 * %c: Caracter
                 * 
                 * char se promociona a int en varargs (ANSI C)
                 * Asi que extraemos int y lo convertimos a char
                 */
                case 'c': {
                    int c = va_arg(args, int);
                    uart_putc((char)c);
                    break;
                }
                
                /**
                 * %s: Cadena (pointer a char)
                 */
                case 's': {
                    char *s = va_arg(args, char *);
                    uart_puts(s);
                    break;
                }
                
                /**
                 * %d: Decimal con signo
                 */
                case 'd': {
                    long d = va_arg(args, long);
                    print_num(d, 10);
                    break;
                }
                
                /**
                 * %x: Hexadecimal sin signo
                 */
                case 'x': {
                    unsigned long x = va_arg(args, unsigned long);
                    print_num((long)x, 16);
                    break;
                }
                
                /**
                 * Especificador desconocido: imprimir literalmente
                 * 
                 * kprintf("Valor %%d: 5");
                 * -> "Valor %d: 5"
                 * (el %% se convierte en %)
                 */
                default:
                    uart_putc('%');
                    uart_putc(*fmt);
                    break;
            }
        } else {
            /**
             * Caracter normal (no '%'): escribir directamente
             */
            uart_putc(*fmt);
        }
        fmt++;  /**< Siguiente caracter de formato */
    }
    va_end(args);
}