/**
 * @file vmm.c
 * @brief Gestor de memoria virtual (Virtual Memory Manager)
 * 
 * @details
 *   Implementa el subsistema de memoria virtual del kernel:
 *   
 *   FUNCIONALIDADES:
 *   - Mapeo de páginas virtuales → físicas (tablas de páginas ARM64)
 *   - Gestión de tablas multinivel (L1/L2/L3)
 *   - Asignación automática de tablas intermedias
 *   - Configuración de permisos (RW, User/Kernel, Exec/NoExec)
 *   
 *   TABLAS DE PÁGINAS ARM64 (4KB pages):
 *   - L1 (PGD): Page Global Directory (bits 38-30 de VA)
 *   - L2 (PMD): Page Middle Directory (bits 29-21 de VA)
 *   - L3 (PTE): Page Table Entry (bits 20-12 de VA)
 *   - Offset: Desplazamiento dentro de la página (bits 11-0)
 *   
 *   INTEGRACIÓN CON DEMAND PAGING:
 *   - map_page() es llamado por handle_fault() cuando hay Page Fault
 *   - Crea tablas intermedias automáticamente si no existen
 *   - Configura permisos según nivel de privilegio (EL0/EL1)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
 */

#include "../../include/mm/vmm.h"
#include "../../include/mm/pmm.h"
#include "../../include/kernel/kutils.h"
#include "../../include/drivers/io.h"

/**
 * @brief Mapea una página virtual a una física
 * @param root_table Puntero a la tabla base (L1/PGD/TTBR0)
 * @param virt Dirección Virtual a mapear
 * @param phys Dirección Física correspondiente
 * @param flags Permisos y atributos (RW, User, SH, MAIR, etc.)
 * 
 * @details
 *   Implementa el mapeo de memoria virtual en ARM64 con tablas de 3 niveles:
 *   
 *   PROCESO DE MAPEO:
 *   1. Extraer índices L1/L2/L3 de la dirección virtual
 *   2. Navegar L1 (PGD):
 *      - Si no existe entrada, crear nueva tabla L2
 *      - Si existe, extraer puntero a L2
 *   3. Navegar L2 (PMD):
 *      - Si no existe entrada, crear nueva tabla L3
 *      - Si existe, extraer puntero a L3
 *   4. Escribir en L3 (PTE):
 *      - Crear descriptor de página: phys | flags | PT_PAGE | MM_ACCESS
 *      - Este descriptor final conecta virtual→física
 *   
 *   DESCRIPTORES ARM64:
 *   - PT_TABLE (L1/L2): Apunta a siguiente nivel
 *   - PT_PAGE (L3): Página física final
 *   - MM_ACCESS: Access Flag (debe estar en 1)
 *   
 *   NOTA: Tras mapear, debe invalidarse la TLB con tlb_invalidate_all()
 */
void map_page(unsigned long *root_table, unsigned long virt, unsigned long phys, unsigned long flags) {
    /* === 1. EXTRAER ÍNDICES DE LA DIRECCIÓN VIRTUAL === */
    unsigned long l1_idx = L1_INDEX(virt);  // Bits 38-30
    unsigned long l2_idx = L2_INDEX(virt);  // Bits 29-21
    unsigned long l3_idx = L3_INDEX(virt);  // Bits 20-12

    /* === 2. NAVEGAR/CREAR TABLA L1 (PGD) === */
    unsigned long *l2_table;
    if (!(root_table[l1_idx] & 1)) {
        /* No existe entrada L1 → Crear nueva tabla L2 */
        unsigned long new_page = get_free_page();
        if (!new_page) {
            kprintf("[VMM] Error: No hay memoria para tabla L2\n");
            return;
        }

        /* Apuntar L1 → L2 (descriptor tipo TABLE) */
        root_table[l1_idx] = new_page | PT_TABLE | 1; // Bit válido
        l2_table = (unsigned long*)new_page;
    } else {
        /* Ya existe, extraer dirección física de L2 (limpiando flags) */
        l2_table = (unsigned long*)(root_table[l1_idx] & 0xFFFFFFFFF000);
    }

    /* === 3. NAVEGAR/CREAR TABLA L2 (PMD) === */
    unsigned long *l3_table;
    if (!(l2_table[l2_idx] & 1)) {
        /* No existe entrada L2 → Crear nueva tabla L3 */
        unsigned long new_page = get_free_page();
        if (!new_page) {
            kprintf("[VMM] Error: No hay memoria para tabla L3\n");
            return;
        }

        /* Apuntar L2 → L3 (descriptor tipo TABLE) */
        l2_table[l2_idx] = new_page | PT_TABLE | 1;
        l3_table = (unsigned long *)new_page;
    } else {
        /* Ya existe, extraer dirección física de L3 */
        l3_table = (unsigned long *)(l2_table[l2_idx] & 0xFFFFFFFFF000);
    }

    /* === 4. ESCRIBIR DESCRIPTOR FINAL EN L3 (PTE) === */
    /* Este descriptor conecta la dirección virtual con la física */
    unsigned long descriptor = phys | PT_PAGE | MM_ACCESS | flags;
    l3_table[l3_idx] = descriptor;

    /* IMPORTANTE: Tras modificar tablas de páginas, se debe invalidar TLB
       La TLB cachea traducciones. Si no se invalida, la MMU puede seguir
       usando la traducción antigua (página no mapeada) → Page Fault */
    /* Nota: tlb_invalidate_all() debe llamarse desde handle_fault() */
}

/* Tabla de páginas principal del kernel (L1/PGD)
   Alineada a 4KB como requiere la MMU de ARM64 */
unsigned long kernel_pgd[512] __attribute__((aligned(4096)));

/**
 * @brief Inicializa el subsistema de memoria virtual
 * 
 * @details
 *   Inicializa la tabla de páginas maestra (kernel_pgd) a cero.
 *   Esta tabla será usada como raíz (L1) para todos los mapeos.
 *   
 *   La dirección de kernel_pgd se carga en TTBR0_EL1 desde boot.S
 */
void init_vmm(void) {
    memset(kernel_pgd, 0, sizeof(kernel_pgd));
    kprintf("[VMM] Inicializando VMM... Tabla Maestra en 0x%x\n", (unsigned long)kernel_pgd);
}