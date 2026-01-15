/**
 * @file malloc.h
 * @brief Interfaz del gestor de heap del kernel
 */

#ifndef MALLOC_H
#define MALLOC_H

#include "../types.h"

/**
 * @brief Inicializa el heap del kernel
 * @param start_addr Dirección inicial del heap
 * @param end_addr   Dirección final (exclusiva) del heap
 */
void kheap_init(unsigned long start_addr, unsigned long end_addr);

/**
 * @brief Reserva memoria (first-fit)
 * @param size Tamaño en bytes
 * @return Puntero asignado o nullptr si falla
 */
void *kmalloc(uint32_t size);

/**
 * @brief Libera un bloque previamente asignado
 * @param ptr Puntero devuelto por kmalloc
 */
void kfree(void *ptr);

#endif // MALLOC_H