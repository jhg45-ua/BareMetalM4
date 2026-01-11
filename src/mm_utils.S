/* 
 * src/mm.S - Manejo de Memoria de Bajo Nivel
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

/* MAIR_EL1: Define los tipos de memoria (Normal vs Device) */
get_mair_el1:
    mrs x0, MAIR_EL1
    ret

set_mair_el1:
    msr MAIR_EL1, x0
    ret

/* TCR_EL1: Configuracion de traduccion (Tamaño de pagina, etc...) */
get_tcr_el1:
    mrs x0, TCR_EL1
    ret

set_tcr_el1:
    msr TCR_EL1, x0
    ret

/* TTBR0_EL1: Puntero a la tabla de paginas de USUARIO (Address 0x0...) */
get_ttbr0_el1:
    mrs x0, TTBR0_EL1
    ret

set_ttbr1_el1:
    msr TTBR1_EL1, x0
    ret

/* TTBR1_EL1: Puntero a la tabla de paginas de KERNEL (Address 0xFFFF...) */
get_ttbr1_el1:
    mrs x0, TTBR1_EL1
    ret

set_ttbr0_el1:
    msr TTBR0_EL1, x0
    ret

/* SCTLR_EL1: Control de sistema (habilitar MMU, caches, etc...) */
get_sctlr_el1:
    mrs x0, SCTLR_EL1
    ret

set_sctlr_el1:
    msr SCTLR_EL1, x0
    ret

/* Invalidate TLB */
tlb_invalidate_all:
    dsb ish         /* Data Sync Barrier */
    tlbi vmalle1is  /* Invalidate all, Inner Shareable */
    dsb ish         /* Data Sync Barrier */
    isb             /* Instruction Sync Barrier */
    ret