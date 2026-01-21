#ifndef VMM_H
#define VMM_H

#define PAGE_SIZE 4096

/* --- Descriptores de Tablas ARM64 --- */
#define PT_TABLE     3     /* Entrada apunta a siguiente nivel (L1, L2) */
#define PT_PAGE      3     /* Entrada apunta a página física (L3) */
#define PT_BLOCK     1     /* Entrada es un bloque grande (L1, L2) */

/* --- Atributos de Memoria (Lower Attributes) --- */
#define MM_ACCESS    (1 << 10) /* Access Flag (AF) - Debe estar a 1 */
#define MM_SH        (3 << 8)  /* Shareable (Inner) */
#define MM_RO        (1 << 7)  /* Read Only */
#define MM_RW        (0 << 7)  /* Read Write */
#define MM_USER      (1 << 6)  /* Accesible por EL0 */
#define MM_KERNEL    (0 << 6)  /* Solo EL1 */
#define MM_EXEC      (0UL << 54) /* Execute Never (XN) = 0 (Ejecutable) */
#define MM_NOEXEC    (1UL << 54) /* Execute Never (XN) = 1 (No Ejecutable) */

/* Índices MAIR (Definidos en mm.c) */
#define ATTR_DEVICE  0
#define ATTR_NORMAL  1

/* Funciones auxiliares para extraer índices de la dirección virtual */
/* ARM64 con 4KB pages usa:
   L1 Index: bits 38-30
   L2 Index: bits 29-21
   L3 Index: bits 20-12
*/
#define L1_INDEX(va) (((va) >> 30) & 0x1FF)
#define L2_INDEX(va) (((va) >> 21) & 0x1FF)
#define L3_INDEX(va) (((va) >> 12) & 0x1FF)

extern unsigned long kernel_pgd[512] __attribute__((aligned(4096)));

void map_page(unsigned long *root_table, unsigned long virt, unsigned long phys, unsigned long flags);
void init_vmm(void);

#endif //VMM_H