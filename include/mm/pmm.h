/**
 * @file pmm.h
 * @brief Interfaz del gestor de memoria física (Physical Memory Manager)
 * 
 * @details
 *   Define funciones para gestionar páginas físicas de memoria:
 *   - Inicialización del PMM
 *   - Asignación y liberación de páginas físicas
 *   - Integración con Demand Paging (get_free_page llamado por handle_fault)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef PMM_H
#define PMM_H

/* ========================================================================== */
/* CONSTANTES DE PAGINACION                                                  */
/* ========================================================================== */

/* Definimos el tamaño de página estandar: 4KB */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* ========================================================================== */
/* FUNCIONES PUBLICAS                                                        */
/* ========================================================================== */

/**
 * @brief Inicializa el gestor de memoria física
 * @param mem_start Dirección donde empieza la RAM libre (después del Kernel)
 * @param mem_size Tamaño de la memoria a gestionar
 */
void pmm_init(unsigned long mem_start, unsigned long mem_size);

/**
 * @brief Busca y reserva una página física libre
 * @return Dirección física de la página (o 0 si no hay memoria)
 * 
 * @details
 *   Asigna una página física de 4KB mediante First-Fit.
 *   Realiza security zeroing antes de devolverla.
 *   
 *   INTEGRACION CON DEMAND PAGING:
 *   Invocado por handle_fault() cuando ocurre un Page Fault
 *   y se necesita asignar memoria bajo demanda.
 */
unsigned long get_free_page(void);

/**
 * @brief Libera una página física
 * @param page Dirección física de la página a liberar
 */
void free_page(unsigned long page);

#endif //PMM_H