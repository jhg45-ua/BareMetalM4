/**
 * @file mm.c
 * @brief Implementacion del subsistema de memoria virtual (MMU)
 * 
 * @details
 *   Configura la MMU de ARM64 con:
 *   - Tablas de paginas L1 (bloques de 1 GB)
 *   - Identity mapping para perifericos y RAM
 *   - Activacion de MMU y caches
 * 
 *   MAPA DE MEMORIA (QEMU virt):
 *   - 0x00000000 - 0x3FFFFFFF: Perifericos (Device memory)
 *   - 0x40000000 - 0x7FFFFFFF: RAM del kernel (Normal memory)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

#include "../../include/mm/mm.h"
#include "../../include/drivers/io.h"
#include "../../include/types.h"

/* ========================================================================== */
/* CONSTANTES DE MEMORIA (MAIR_EL1)                                         */
/* ========================================================================== */

/* Tipos de memoria */
#define MT_DEVICE_nGnRnE 0x00  /* Device: sin cache, sin reordenamiento */
#define MT_NORMAL_NC     0x44  /* Normal sin cache */
#define MT_NORMAL        0xFF  /* Normal con cache */

/* Registro MAIR: 3 tipos en indices 0, 1 y 2 */
#define MAIR_VALUE \
    (0x00UL << (0 * 8)) | \
    (0x44UL << (1 * 8)) | \
    (0xFFUL << (2 * 8))

/* ========================================================================== */
/* CONFIGURACION DE TRADUCCION (TCR_EL1)                                    */
/* ========================================================================== */

/* T0SZ = 25 -> 39 bits de espacio virtual (512 GB)
   Con 39 bits, la traduccion empieza en NIVEL 1 (bloques de 1GB) */
#define TCR_T0SZ        (64 - 39)
#define TCR_T1SZ        (64 - 39)
#define TCR_TG0_4K      (0UL << 14)  /* Granularidad 4 KB */
#define TCR_TG1_4K      (2UL << 30)
#define TCR_VALUE       (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4K | TCR_TG1_4K)

/* ========================================================================== */
/* DESCRIPTORES DE TABLA                                                     */
/* ========================================================================== */

#define PT_TABLE     0x3        /* Entrada de tabla (apunta a siguiente nivel) */
#define PT_BLOCK     0x1        /* Entrada de bloque (mapeo directo) */
#define PT_AF        (1UL<<10)  /* Access Flag */
#define PT_ISH       (3UL<<8)   /* Inner Shareable */

/* ========================================================================== */
/* ATRIBUTOS DE MEMORIA                                                      */
/* ========================================================================== */

/* RAM: Bloque + AF + ISH + indice MAIR 2 (Normal con cache) */
#define MM_NORMAL    (PT_BLOCK | PT_AF | PT_ISH | (2UL << 2) | (0UL << 50))

/* Perifericos: Bloque + AF + ISH + indice MAIR 0 (Device) */
#define MM_DEVICE    (PT_BLOCK | PT_AF | PT_ISH | (0UL << 2) | (0UL << 50))

/* ========================================================================== */
/* TABLA DE PAGINAS L1                                                       */
/* ========================================================================== */

/* Tabla L1: 512 entradas, cada una cubre 1 GB (total 512 GB) */
uint64_t page_table_l1[512] __attribute__((aligned(4096)));

/* ========================================================================== */
/* INICIALIZACION DE LA MMU                                                  */
/* ========================================================================== */

/**
 * @brief Inicializa la MMU y activa memoria virtual
 * 
 * Secuencia:
 * 1. Crear tabla de paginas L1 (bloques de 1GB)
 * 2. Mapear perifericos (Device) y RAM (Normal)
 * 3. Configurar registros (MAIR, TCR, TTBR)
 * 4. Activar MMU y caches
 */
void mem_init() {
    kprintf("   [MMU] Inicializando Tablas (Bloques de 1GB)...\n");

    /* ====================================================================== */
    /* 1. LIMPIAR TABLA                                                      */
    /* ====================================================================== */

    for (int i = 0; i < 512; i++) {
        page_table_l1[i] = 0;
    }

    /* ====================================================================== */
    /* 2. MAPEAR MEMORIA (IDENTITY MAPPING)                                 */
    /* ====================================================================== */
    
    /* ENTRADA 0: [0x00000000 - 0x3FFFFFFF] (1 GB)
       Perifericos: UART (0x09000000), GIC (0x08000000) */
    page_table_l1[0] = 0x00000000UL | MM_DEVICE;

    /* ENTRADA 1: [0x40000000 - 0x7FFFFFFF] (1 GB)
       RAM del kernel: codigo, stack, datos */
    page_table_l1[1] = 0x40000000UL | MM_NORMAL;

    /* ====================================================================== */
    /* 3. CONFIGURAR REGISTROS                                               */
    /* ====================================================================== */

    set_mair_el1(MAIR_VALUE);
    set_tcr_el1(TCR_VALUE);
    set_ttbr0_el1((unsigned long)page_table_l1);
    set_ttbr1_el1((unsigned long)page_table_l1);

    /* ====================================================================== */
    /* 4. ACTIVAR MMU Y CACHES                                               */
    /* ====================================================================== */

    kprintf("   [MMU] Activando Traduccion y Caches...\n");

    unsigned long sctlr = get_sctlr_el1();
    sctlr |= 1;       /* M: MMU Enable */
    sctlr |= (1<<2);  /* C: D-Cache Enable */
    sctlr |= (1<<12); /* I: I-Cache Enable */
    
    set_sctlr_el1(sctlr);
    tlb_invalidate_all();

    kprintf("   [MMU] Sistema operativo vivo en memoria virtual.\n");
}