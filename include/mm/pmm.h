#ifndef PMM_H
#define PMM_H

/* Definimos el tamaño de página estandar: 4KB */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* Puntero al inicio de la memoria libre (después del kernel) */
/* Lo definimos dinámicamente */

void pmm_init(unsigned long mem_start, unsigned long mem_size);
unsigned long get_free_page(void);
void free_page(unsigned long page);

#endif //PMM_H