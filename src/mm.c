/* src/mm.c */
#include "../include/mm.h"
#include "../include/io.h"
#include "../include/types.h"

/* --- CONSTANTES DE MEMORIA --- */
/* MAIR_EL1: Tipos de memoria */
#define MT_DEVICE_nGnRnE 0x00
#define MT_NORMAL_NC     0x44
#define MT_NORMAL        0xFF

/* Fix del Warning: Usamos '0UL' o '(unsigned long)0' para operar en 64 bits */
#define MAIR_VALUE \
    (0x00UL << (0 * 8)) | \
    (0x44UL << (1 * 8)) | \
    (0xFFUL << (2 * 8))

/* TCR_EL1: Configuración IMPORTANTE
   T0SZ = 25 -> (64 - 25) = 39 bits de espacio virtual (512 GB).
   Con 39 bits, la traducción empieza en NIVEL 1 (L1).
   Esto nos permite usar Bloques de 1GB directamente en la tabla raíz. */
#define TCR_T0SZ        (64 - 39) 
#define TCR_T1SZ        (64 - 39)
#define TCR_TG0_4K      (0UL << 14)
#define TCR_TG1_4K      (2UL << 30)
#define TCR_VALUE       (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4K | TCR_TG1_4K)

/* Descriptores de Tabla */
#define PT_TABLE     0x3
#define PT_BLOCK     0x1
#define PT_AF        (1UL<<10) /* Access Flag */
#define PT_ISH       (3UL<<8)  /* Shareability */

/* Fix del Warning: Casteo explícito a unsigned long para el shift de 50 */
/* RAM (Normal) */
#define MM_NORMAL    (PT_BLOCK | PT_AF | PT_ISH | (2UL << 2) | (0UL << 50))
/* Periféricos (Device) */
#define MM_DEVICE    (PT_BLOCK | PT_AF | PT_ISH | (0UL << 2) | (0UL << 50))

/* --- TABLA DE PÁGINAS (NIVEL 1) --- */
/* Una tabla L1 tiene 512 entradas. Cada entrada cubre 1GB. */
uint64_t page_table_l1[512] __attribute__((aligned(4096)));

void mem_init() {
    kprintf("   [MMU] Inicializando Tablas (Bloques de 1GB)...\n");

    /* 1. Limpiar tabla */
    for (int i = 0; i < 512; i++) {
        page_table_l1[i] = 0;
    }

    /* 2. Mapear Memoria (Identity Mapping) */
    
    /* ENTRADA 0: [0x00000000 - 0x3FFFFFFF] (0GB a 1GB)
       Aquí están el UART (0x09000000) y el GIC.
       Lo marcamos como DEVICE. */
    page_table_l1[0] = 0x00000000UL | MM_DEVICE;

    /* ENTRADA 1: [0x40000000 - 0x7FFFFFFF] (1GB a 2GB)
       Aquí está la RAM del Kernel en QEMU 'virt'.
       ¡ESTO ES LO QUE FALTABA! Sin esto, el kernel no se ve a sí mismo. */
    page_table_l1[1] = 0x40000000UL | MM_NORMAL;

    /* 3. Configurar Registros */
    set_mair_el1(MAIR_VALUE);
    set_tcr_el1(TCR_VALUE);

    /* Apuntamos a nuestra tabla L1 */
    set_ttbr0_el1((unsigned long)page_table_l1);
    set_ttbr1_el1((unsigned long)page_table_l1);

    /* 4. Activar MMU */
    kprintf("   [MMU] Activando Traduccion y Caches...\n");

    unsigned long sctlr = get_sctlr_el1();
    sctlr |= 1;       /* M: MMU Enable */
    sctlr |= (1<<2);  /* C: D-Cache Enable */
    sctlr |= (1<<12); /* I: I-Cache Enable */
    
    /* Invocamos la función de asm para aplicar cambios */
    set_sctlr_el1(sctlr);
    tlb_invalidate_all();

    kprintf("   [MMU] Sistema operativo vivo en memoria virtual.\n");
}