/**
 * @file vmm.h
 * @brief Interfaz del gestor de memoria virtual (VMM)
 * 
 * @details
 *   Define constantes, macros y funciones para gestión de memoria virtual ARM64:
 *   
 *   TABLAS DE PÁGINAS:
 *   - Descriptores ARM64 (PT_TABLE, PT_PAGE, PT_BLOCK)
 *   - Atributos de memoria (permisos, shareability, ejecutabilidad)
 *   - Macros para extraer índices L1/L2/L3 de direcciones virtuales
 *   
 *   INTEGRACIÓN CON DEMAND PAGING:
 *   - map_page() crea mapeos virtual→física bajo demanda
 *   - Usado por handle_fault() en Page Faults
 *   - Asignación automática de tablas intermedias
 *   
 *   CARACTERÍSTICAS ARM64:
 *   - Páginas de 4KB (PAGE_SIZE = 4096)
 *   - Traducción de 3 niveles (L1/L2/L3)
 *   - 48 bits de espacio de direcciones virtuales
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef VMM_H
#define VMM_H

#define PAGE_SIZE 4096

/* ========================================================================== */
/* DESCRIPTORES DE TABLAS ARM64                                              */
/* ========================================================================== */

/**
 * @brief Descriptores de tablas de páginas ARM64
 * 
 * PT_TABLE (=3): Entrada apunta a tabla de siguiente nivel (L1→L2, L2→L3)
 * PT_PAGE (=3):  Entrada final apunta a página física (L3)
 * PT_BLOCK (=1): Entrada es un bloque grande 2MB/1GB (L1/L2)
 */
#define PT_TABLE     3     /* Entrada apunta a siguiente nivel (L1, L2) */
#define PT_PAGE      3     /* Entrada apunta a página física (L3) */
#define PT_BLOCK     1     /* Entrada es un bloque grande (L1, L2) */

/* ========================================================================== */
/* ATRIBUTOS DE MEMORIA (Lower Attributes)                                  */
/* ========================================================================== */

/**
 * @brief Atributos de páginas ARM64
 * 
 * MM_ACCESS: Access Flag (AF) - Debe estar en 1 para páginas válidas
 * MM_SH: Shareability (Inner) - Para coherencia de cache multicore
 * MM_RO/RW: Permisos de escritura
 * MM_USER/KERNEL: Accesible desde EL0 o solo EL1
 * MM_EXEC/NOEXEC: Execute Never (XN) - Previene ejecución de código
 */
#define MM_ACCESS    (1 << 10) /* Access Flag (AF) - Debe estar a 1 */
#define MM_SH        (3 << 8)  /* Shareable (Inner) */
#define MM_RO        (1 << 7)  /* Read Only */
#define MM_RW        (0 << 7)  /* Read Write */
#define MM_USER      (1 << 6)  /* Accesible por EL0 (usuario) */
#define MM_KERNEL    (0 << 6)  /* Solo EL1 (kernel) */
#define MM_EXEC      (0UL << 54) /* Execute Never (XN) = 0 (Ejecutable) */
#define MM_NOEXEC    (1UL << 54) /* Execute Never (XN) = 1 (No Ejecutable) */

/* ========================================================================== */
/* INDICES MAIR (Memory Attribute Indirection Register)                     */
/* ========================================================================== */

/**
 * @brief Índices MAIR para tipos de memoria
 * 
 * ATTR_DEVICE: Memoria de dispositivos (MMIO) - No cacheable
 * ATTR_NORMAL: Memoria normal (RAM) - Cacheable
 * 
 * Definidos en mm.c durante la inicialización de la MMU
 */
#define ATTR_DEVICE  0
#define ATTR_NORMAL  1

/* ========================================================================== */
/* MACROS PARA ÍNDICES DE TABLA (Extracción de bits de VA)                 */
/* ========================================================================== */

/**
 * @brief Macros para extraer índices de dirección virtual ARM64
 * 
 * ARM64 con páginas de 4KB usa traducción de 3 niveles:
 * - L1 Index (PGD): bits 38-30 → 512 entradas
 * - L2 Index (PMD): bits 29-21 → 512 entradas
 * - L3 Index (PTE): bits 20-12 → 512 entradas
 * - Offset:         bits 11-0  → 4096 bytes
 * 
 * Total: 512 × 512 × 512 × 4096 = 512GB de espacio virtual
 */
#define L1_INDEX(va) (((va) >> 30) & 0x1FF)  /* Bits 38-30 */
#define L2_INDEX(va) (((va) >> 21) & 0x1FF)  /* Bits 29-21 */
#define L3_INDEX(va) (((va) >> 12) & 0x1FF)  /* Bits 20-12 */

/* ========================================================================== */
/* VARIABLES GLOBALES                                                        */
/* ========================================================================== */

/**
 * @brief Tabla de páginas principal del kernel (L1/PGD)
 * 
 * Array de 512 entradas de 64 bits (4KB total)
 * Alineado a 4KB como requiere la arquitectura ARM64
 * Se carga en TTBR0_EL1 durante el boot
 */
extern unsigned long kernel_pgd[512] __attribute__((aligned(4096)));

/* ========================================================================== */
/* FUNCIONES PÚBLICAS                                                        */
/* ========================================================================== */

/**
 * @brief Mapea una página virtual a una física
 * @param root_table Puntero a la tabla base (L1/PGD/TTBR0)
 * @param virt Dirección Virtual a mapear (será alineada a 4KB)
 * @param phys Dirección Física correspondiente (debe estar alineada a 4KB)
 * @param flags Permisos y atributos (MM_RW|MM_USER|MM_SH|ATTR_NORMAL, etc.)
 * 
 * @details
 *   Crea o actualiza una entrada en las tablas de páginas.
 *   Crea automáticamente tablas intermedias (L2/L3) si no existen.
 *   Usa get_free_page() del PMM para nuevas tablas.
 *   
 *   IMPORTANTE: Tras llamar a map_page(), debe invalidarse la TLB
 *   con tlb_invalidate_all() para que la MMU vea los cambios.
 */
void map_page(unsigned long *root_table, unsigned long virt, unsigned long phys, unsigned long flags);

/**
 * @brief Inicializa el subsistema de memoria virtual
 * 
 * @details
 *   Inicializa kernel_pgd a cero.
 *   Debe llamarse antes de usar map_page().
 */
void init_vmm(void);

#endif //VMM_H