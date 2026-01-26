/**
 * @file kutils.c
 * @brief Utilidades del kernel
 * 
 * @details
 *   Implementación de funciones de utilidad general del kernel
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.6
 */

#include "../../include/utils/kutils.h"
#include "../../include/drivers/io.h"

/* ========================================================================== */
/* HERRAMIENTAS BASE DEL KERNEL                                             */
/* ========================================================================== */

/**
 * @brief Detiene el sistema en caso de error crítico
 * @param msg Mensaje de error a mostrar
 */
void panic(const char *msg) {
    kprintf("\n!!!! KERNEL PANIC !!!!\n");
    kprintf(msg);
    kprintf("\nSistema detenido");
    while(1);
}

/**
 * @brief Retardo activo (busy-wait)
 * @param count Número de iteraciones
 */
void delay(int count) {
    for (volatile int i = 0; i < count; i++);
}

/**
 * @brief Compara dos cadenas de caracteres
 * @param s1 Primera cadena
 * @param s2 Segunda cadena
 * @return 0 si son iguales, diferencia entre caracteres si no
 */
int k_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * @brief Copia una cadena con límite de longitud
 * @param dst Destino
 * @param src Origen
 * @param max_len Longitud máxima (incluyendo null terminator)
 */
void k_strncpy(char *dst, const char *src, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/**
 * @brief Calcula la longitud de una cadena de texto
 * @param str Puntero a la cadena (terminada en '\0')
 * @return Número de caracteres antes del terminador nulo
 */
int k_strlen(const char *str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * @brief Rellena un bloque de memoria con un valor específico
 * @param s Puntero al bloque de memoria
 * @param c Valor a escribir (convertido a unsigned char)
 * @param n Número de bytes a rellenar
 * @return Puntero al bloque de memoria (s)
 */
void *memset(void *s, int c, unsigned long n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

/**
 * @brief Copia un bloque de memoria de origen a destino
 * @param dest Puntero al destino
 * @param src Puntero al origen
 * @param n Número de bytes a copiar
 * @return Puntero al destino (dest)
 */
void *memcpy(void *dest, const void *src, unsigned long n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
