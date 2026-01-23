/**
 * @file mm.h
 * @brief Interfaz para el subsistema de gestion de memoria (MMU)
 * 
 * @details
 *   Define funciones para configurar la MMU, tablas de paginas,
 *   registros de sistema ARM64 y el TLB.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef MM_H
#define MM_H

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Assembly - mm_utils.S)                               */
/* ========================================================================== */

/* Registros de tablas de paginas */
extern void set_ttbr0_el1(unsigned long addr);
extern void set_ttbr1_el1(unsigned long addr);

/* Configuracion de tipos de memoria */
extern void set_mair_el1(unsigned long value);

/* Configuracion de traduccion */
extern void set_tcr_el1(unsigned long value);

/* Control del sistema (MMU, caches) */
extern void set_sctlr_el1(unsigned long value);
extern unsigned long get_sctlr_el1(void);

/* Translation Lookaside Buffer */
extern void tlb_invalidate_all(void);

/* ========================================================================== */
/* INICIALIZACION DE MEMORIA                                                 */
/* ========================================================================== */

/**
 * @brief Inicializa la MMU y activa memoria virtual
 * 
 * Configura tablas de páginas, mapea perifericos y RAM,
 * y activa la MMU con cachés.
 */
void mem_init(unsigned long heap_start, unsigned long heap_size);

/**
 * @brief Inicializa el subsistema de gestión de memoria
 * 
 * Configura estructuras de datos para la gestión dinámica
 * de memoria (kmalloc/kfree).
 */
void init_memory_system();

#endif // MM_H