/**
 * @file types.h
 * @brief Definiciones de tipos de datos de tamaño fijo para ARM64
 * 
 * @details
 *   En ARM64:
 *   - char: 8 bits
 *   - short: 16 bits
 *   - int: 32 bits
 *   - long: 64 bits (diferente a x86 donde es 32 bits)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.6
 */

#ifndef TYPES_H
#define TYPES_H

/* Entero sin signo de 8 bits (0 a 255) */
typedef unsigned char      uint8_t;

/* Entero sin signo de 16 bits (0 a 65,535) */
typedef unsigned short     uint16_t;

/* Entero sin signo de 32 bits (0 a 4,294,967,295) */
typedef unsigned int       uint32_t;

/* Entero sin signo de 64 bits (0 a 18,446,744,073,709,551,615) */
typedef unsigned long      uint64_t;

/* Puntero nulo estándar para C */
#ifndef NULL
#define NULL ((void*)0)
#endif

#endif // TYPES_H