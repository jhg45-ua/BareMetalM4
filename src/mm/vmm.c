/**
 * @file vmm.c
 * @brief Gestor de memoria virtual (Virtual Memory Manager)
 * 
 * @details
 *   Implementa el gestor de memoria virtual del kernel:
 *   - Mapeo de páginas virtuales a físicas
 *   - Gestión de tablas de páginas multinivel (L1/L2/L3)
 *   - Inicialización del subsistema VMM
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#include "../../include/mm/vmm.h"
#include "../../include/mm/pmm.h"
#include "../../include/kernel/kutils.h"
#include "../../include/drivers/io.h"

/**
 * @brief Mapea una página virtual a una física
 * @param root_table Puntero a la tabla base (L1/TTBR)
 * @param virt Dirección Virtual
 * @param phys Dirección Física
 * @param flags Permisos (RW, User, etc.)
 */

void map_page(unsigned long *root_table, unsigned long virt, unsigned long phys, unsigned long flags) {
    unsigned long l1_idx = L1_INDEX(virt);
    unsigned long l2_idx = L2_INDEX(virt);
    unsigned long l3_idx = L3_INDEX(virt);

    /* 1. Revisar L1 (PGD) */
    unsigned long *l2_table;
    if (!(root_table[l1_idx] & 1)) {
        /* No existe entrada L1 -> Crear nueva tabla L2 */
        unsigned long new_page = get_free_page();
        if (!new_page) {
            kprintf("[VMM] Error: No hay memoria para tabla L2\n");
            return;
        }

        /* Apuntamos L1 -> L2 (PT_TABLE = 3) */
        root_table[l1_idx] = new_page | PT_TABLE | 1; // Bit valido
        l2_table = (unsigned long*)new_page;
    } else {
        /* Ya existe, extraemos la direccion fisica (limpiando flags) */
        l2_table = (unsigned long*)(root_table[l1_idx] & 0xFFFFFFFFF000);
    }

    /* 2. Revisar L2 (PMD) */
    unsigned long *l3_table;
    if (!(l2_table[l2_idx] & 1)) {
        /* No existe entrada L2 -> Crear nueva tabla L3 */
        unsigned long new_page = get_free_page();
        if (!new_page) {
            kprintf("[VMM] Error: No hay memoria para tabla L3\n");
            return;
        }

        l2_table[l2_idx] = new_page | PT_TABLE | 1;
        l3_table = (unsigned long *)new_page;
    } else {
        l3_table = (unsigned long *)(l2_table[l2_idx] & 0xFFFFFFFFF000);
    }

    /* 3. Escribir en L3 (PTE) - La entrada final */
    /* Descriptor de página: Dirección Física + Atributos + Valid(3) */
    unsigned long descriptor = phys | PT_PAGE | MM_ACCESS | flags;

    l3_table[l3_idx] = descriptor;

    /* Nota: En un sistema real, aquí habría que invalidar TLB (dsb ish; tlbi...) */
}

unsigned long kernel_pgd[512] __attribute__((aligned(4096)));

void init_vmm(void) {
    memset(kernel_pgd, 0, sizeof(kernel_pgd));
    kprintf("[VMM] Inicializando VMM... Tabla Maestra en 0x%x\n", (unsigned long)kernel_pgd);
}