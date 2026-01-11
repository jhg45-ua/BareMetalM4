/**
 * @file mm.h
 * @brief Interfaz para el subsistema de gestion de memoria (MMU)
 * 
 * Define funciones para configurar la MMU, tablas de paginas,
 * registros de sistema ARM64 y el TLB.
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
 * Configura tablas de paginas, mapea perifericos y RAM,
 * y activa la MMU con caches.
 */
void mem_init(void);

#endif // MM_H