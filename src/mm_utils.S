/**
 * @file mm_utils.S
 * @brief Funciones de bajo nivel para gestion de memoria (Assembly ARM64)
 * 
 * @details
 *   Implementa acceso a registros de sistema (mrs/msr):
 *   - MAIR_EL1: Tipos de memoria (Device, Normal)
 *   - TCR_EL1: Configuracion de traduccion
 *   - TTBR0/1_EL1: Tablas de paginas
 *   - SCTLR_EL1: Control del sistema (MMU, caches)
 *   - TLB: Cache de traducciones
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

.global get_mair_el1
.global set_mair_el1
.global get_tcr_el1
.global set_tcr_el1
.global get_ttbr0_el1
.global set_ttbr0_el1
.global get_ttbr1_el1
.global set_ttbr1_el1
.global get_sctlr_el1
.global set_sctlr_el1
.global tlb_invalidate_all

/* ========================================================================== */
/* MAIR_EL1: MEMORY ATTRIBUTE INDIRECTION REGISTER                          */
/* ========================================================================== */

/* Lee el registro MAIR_EL1 (tipos de memoria: Device, Normal) */
get_mair_el1:
    mrs x0, MAIR_EL1
    ret

/* Escribe el registro MAIR_EL1 */
set_mair_el1:
    msr MAIR_EL1, x0
    ret

/* ========================================================================== */
/* TCR_EL1: TRANSLATION CONTROL REGISTER                                    */
/* ========================================================================== */

/* Lee el registro TCR_EL1 (parametros de traduccion) */
get_tcr_el1:
    mrs x0, TCR_EL1
    ret

/* Escribe el registro TCR_EL1 (T0SZ, TG0, granularidad, etc.) */
set_tcr_el1:
    msr TCR_EL1, x0
    ret

/* ========================================================================== */
/* TTBR0_EL1 y TTBR1_EL1: TRANSLATION TABLE BASE REGISTERS                  */
/* ========================================================================== */

/* Lee TTBR0_EL1 (tabla de paginas para direcciones bajas 0x0...) */
get_ttbr0_el1:
    mrs x0, TTBR0_EL1
    ret

/* Escribe TTBR0_EL1 (debe estar alineado a 4KB) */
set_ttbr0_el1:
    msr TTBR0_EL1, x0
    ret

/* Lee TTBR1_EL1 (tabla de paginas para direcciones altas 0xFFFF...) */
get_ttbr1_el1:
    mrs x0, TTBR1_EL1
    ret

/* Escribe TTBR1_EL1 */
set_ttbr1_el1:
    msr TTBR1_EL1, x0
    ret

/* ========================================================================== */
/* SCTLR_EL1: SYSTEM CONTROL REGISTER                                       */
/* ========================================================================== */

/* Lee SCTLR_EL1 (control de MMU, caches, etc.) */
get_sctlr_el1:
    mrs x0, SCTLR_EL1
    ret

/* Escribe SCTLR_EL1 (bit 0: MMU, bit 2: D-Cache, bit 12: I-Cache) */
set_sctlr_el1:
    msr SCTLR_EL1, x0
    ret

/* ========================================================================== */
/* TLB: TRANSLATION LOOKASIDE BUFFER                                         */
/* ========================================================================== */

/* Invalida todas las entradas del TLB
   
   El TLB cachea traducciones virtuales->fisicas.
   Debe invalidarse tras modificar tablas de paginas.
   
   Secuencia:
   1. dsb ish: Sincronizar escrituras a memoria
   2. tlbi vmalle1is: Invalidar TLB en todos los cores
   3. dsb ish: Asegurar invalidacion completa
   4. isb: Sincronizar pipeline de instrucciones */
tlb_invalidate_all:
    dsb ish         /* Data Sync Barrier - Inner Shareable */
    tlbi vmalle1is  /* TLB Invalidate All, EL1, Inner Shareable */
    dsb ish         /* Data Sync Barrier - Inner Shareable */
    isb             /* Instruction Sync Barrier */
    ret