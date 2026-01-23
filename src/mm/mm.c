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
 * @version 0.4
 */

#include "../../include/mm/mm.h"
#include "../../include/mm/vmm.h"
#include "../../include/mm/pmm.h"
#include "../../include/mm/malloc.h"
#include "../../include/drivers/io.h"
#include "../../include/types.h"

/* ========================================================================== */
/* CONSTANTES MAIR Y TCR                                                      */
/* ========================================================================== */

#define MAIR_VALUE \
(0x00UL << (0 * 8)) | \
(0x44UL << (1 * 8)) | \
(0xFFUL << (2 * 8))

#define TCR_T0SZ        (64 - 39)
#define TCR_T1SZ        (64 - 39)
#define TCR_TG0_4K      (0UL << 14)
#define TCR_TG1_4K      (2UL << 30)
#define TCR_SH_IS       (3UL << 12) | (3UL << 28)
#define TCR_ORGN_WB     (1UL << 10) | (1UL << 26)
#define TCR_IRGN_WB     (1UL << 8)  | (1UL << 24)

#define TCR_VALUE       (TCR_T0SZ | TCR_T1SZ | TCR_TG0_4K | TCR_TG1_4K | \
TCR_SH_IS | TCR_ORGN_WB | TCR_IRGN_WB)

/* ========================================================================== */
/* BANDERAS DE MAPEO PARA MAP_PAGE                                          */
/* ========================================================================== */

/* Flags = Shareable + Read/Write + Kernel Mode + Índice MAIR */
#define FLAGS_NORMAL (MM_SH | MM_RW | MM_KERNEL | (ATTR_NORMAL << 2))
#define FLAGS_DEVICE (MM_SH | MM_RW | MM_KERNEL | (ATTR_DEVICE << 2))

extern char _end; /* Símbolo del linker donde termina el kernel */

/* ========================================================================== */
/* INICIALIZACION DE LA MMU (NUEVA VERSION VMM)                             */
/* ========================================================================== */

/**
 * @brief Inicializa la MMU y activa memoria virtual
 * 
 * Secuencia:
 * 1. Crear tabla de páginas L1 (bloques de 1GB)
 * 2. Mapear perifericos (Device) y RAM (Normal)
 * 3. Configurar registros (MAIR, TCR, TTBR)
 * 4. Activar MMU y caches
 */
void mem_init(unsigned long heap_start, unsigned long heap_size) {
    kprintf("   [MMU] Mapeando Kernel y Perifericos con paginas de 4KB...\n");

    /* 1. Mapear Periféricos (UART y Controlador de Interrupciones) */
    map_page(kernel_pgd, 0x09000000, 0x09000000, FLAGS_DEVICE); /* UART */
    map_page(kernel_pgd, 0x08000000, 0x08000000, FLAGS_DEVICE); /* GIC Distrib */
    map_page(kernel_pgd, 0x08010000, 0x08010000, FLAGS_DEVICE); /* GIC CPU IF */

    /* 2. Mapear TODA la RAM (Identity mapping 1:1) */
    /* QEMU virt nos da 128MB de RAM empezando en 0x40000000 */
    unsigned long start_ram = 0x40000000;
    unsigned long total_ram_size = 128 * 1024 * 1024; /* 128MB */
    unsigned long end_ram = start_ram + total_ram_size;

    for (unsigned long addr = start_ram; addr < end_ram; addr += PAGE_SIZE) {
        map_page(kernel_pgd, addr, addr, FLAGS_NORMAL);
    }

    /* 3. Configurar Registros con la nueva tabla MAESTRA (kernel_pgd) */
    set_mair_el1(MAIR_VALUE);
    set_tcr_el1(TCR_VALUE);
    set_ttbr0_el1((unsigned long)kernel_pgd);
    set_ttbr1_el1((unsigned long)kernel_pgd);

    /* 4. Activar MMU y Caches */
    kprintf("   [MMU] Activando Traduccion Avanzada...\n");
    unsigned long sctlr = get_sctlr_el1();
    sctlr |= 1;       /* M: MMU Enable */
    sctlr |= (1<<2);  /* C: D-Cache Enable */
    sctlr |= (1<<12); /* I: I-Cache Enable */

    set_sctlr_el1(sctlr);
    tlb_invalidate_all();

    kprintf("   [MMU] Sistema estable en modo 39-bits/4KB.\n");
}

/**
 * @brief Inicializa el sistema completo de memoria
 * 
 * Configura:
 * 1. MMU (paginación y memoria virtual)
 * 2. Heap del kernel (gestor de memoria dinámica)
 */
void init_memory_system() {
    unsigned long heap_start = (unsigned long)&_end;
    unsigned long heap_size  = 64 * 1024 * 1024; /* 64MB para Heap */
    unsigned long mem_total  = 128 * 1024 * 1024; /* 128MB RAM QEMU */

    /* El PMM gestionará la memoria libre que queda DESPUÉS del Heap */
    unsigned long pmm_start = heap_start + heap_size;
    unsigned long pmm_size  = mem_total - (pmm_start - 0x40000000);

    /* 1. Iniciar estructuras */
    pmm_init(pmm_start, pmm_size);
    init_vmm(); /* Limpia el kernel_pgd */

    /* 2. Mapear y Activar MMU */
    mem_init(heap_start, heap_size);

    /* 3. Iniciar HEAP ya con la MMU encendida */
    kheap_init(heap_start, heap_start + heap_size);

    kprintf("   [MEM] Subsistema de memoria (PMM + VMM + MMU + Heap) listo.\n");
}
