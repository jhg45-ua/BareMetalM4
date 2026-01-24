/**
 * @file kutils.h
 * @brief Utilidades del kernel
 * 
 * @details
 *   Funciones de utilidad general del kernel:
 *   - Panic del sistema
 *   - Delays
 *   - Manipulación de strings
 */

#ifndef KUTILS_H
#define KUTILS_H

/**
 * @brief Detiene el sistema en caso de error crítico
 * @param msg Mensaje de error a mostrar
 */
void panic(const char *msg);

/**
 * @brief Retardo activo (busy-wait)
 * @param count Número de iteraciones
 */
void delay(int count);

/**
 * @brief Compara dos cadenas de caracteres
 * @param s1 Primera cadena
 * @param s2 Segunda cadena
 * @return 0 si son iguales, diferencia entre caracteres si no
 */
int k_strcmp(const char *s1, const char *s2);

/**
 * @brief Copia una cadena con límite de longitud
 * @param dst Destino
 * @param src Origen
 * @param max_len Longitud máxima (incluyendo null terminator)
 */
void k_strncpy(char *dst, const char *src, int max_len);

/**
 * @brief Calcula la longitud de una cadena de texto
 * @param str Puntero a la cadena (terminada en '\0')
 * @return Número de caracteres antes del terminador nulo
 */
int k_strlen(const char *str);

/**
 * @brief Rellena un bloque de memoria con un valor
 * @param s Puntero al bloque
 * @param c Valor a escribir
 * @param n Número de bytes
 * @return Puntero al bloque (s)
 */
void *memset(void *s, int c, unsigned long n);

/**
 * @brief Copia un bloque de memoria
 * @param dest Destino
 * @param src Origen
 * @param n Número de bytes
 * @return Puntero al destino
 */
void *memcpy(void *dest, const void *src, unsigned long n);

#endif /* KUTILS_H */
