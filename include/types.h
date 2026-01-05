/**
 * @file types.h
 * @brief Definiciones de tipos de datos de tamaño fijo para ARM64
 * 
 * @details
 *   Este archivo define tipos enteros de ancho especificado,
 *   independientemente de la arquitectura. Asegura portabilidad
 *   y comportamiento predecible en ARM64.
 * 
 *   En ARM64:
 *   - char: 8 bits
 *   - short: 16 bits
 *   - int: 32 bits
 *   - long: 64 bits (diferente a x86 donde es 32 bits)
 * 
 * @author Sistema Operativo Educativo
 * @version 1.0
 */

#ifndef TYPES_H
#define TYPES_H

/**
 * @typedef uint8_t
 * @brief Entero sin signo de 8 bits (1 byte)
 * 
 * Rango: 0 a 255
 * Usos: Caracteres, bytes individuales, flags de 8 bits
 */
typedef unsigned char      uint8_t;

/**
 * @typedef uint16_t
 * @brief Entero sin signo de 16 bits (2 bytes)
 * 
 * Rango: 0 a 65,535
 * Usos: Valores de 16 bits, direcciones parciales
 */
typedef unsigned short     uint16_t;

/**
 * @typedef uint32_t
 * @brief Entero sin signo de 32 bits (4 bytes)
 * 
 * Rango: 0 a 4,294,967,295
 * Usos: Registros de hardware MMIO, contadores de 32 bits
 */
typedef unsigned int       uint32_t;

/**
 * @typedef uint64_t
 * @brief Entero sin signo de 64 bits (8 bytes)
 * 
 * Rango: 0 a 18,446,744,073,709,551,615
 * Usos: Direcciones de memoria, contadores de 64 bits, registros ARM64
 * 
 * @note En ARM64, 'unsigned long' son 64 bits (diferente a x86)
 *       Por eso definimos explicitamente uint64_t para claridad
 */
typedef unsigned long      uint64_t;

#endif // TYPES_H