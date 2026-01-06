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

#endif /* KUTILS_H */
