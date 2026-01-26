/**
 * @file pmm.c
 * @brief Gestor de memoria física (Physical Memory Manager)
 * 
 * @details
 *   Implementa el gestor de memoria física del kernel:
 *   - Asignación de páginas físicas mediante bitmap
 *   - Algoritmo First-Fit para búsqueda de páginas libres
 *   - Gestión de 128MB de RAM
 *   - Integración con Demand Paging: get_free_page() es invocado por
 *     handle_fault() cuando se produce un Page Fault y se necesita
 *     asignar una página física bajo demanda
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.6
 */

#include "../../include/mm/pmm.h"
#include "../../include/utils/kutils.h"
#include "../../include/drivers/io.h"

/* Gestionaremos 128MB de RAM para usuarios por ahora */
#define MEMORY_SIZE (128 * 1024 * 1024)
#define TOTAL_PAGES (MEMORY_SIZE / PAGE_SIZE)

/*
 * Mapa de bits: Cada bit representa una página de 4KB.
 * Si TOTAL_PAGES = 32768 (128MB), necesitamos 32768 bits.
 * 32768 bits / 8 bits/byte = 4096 bytes para el mapa.
 */
static unsigned char mem_map[TOTAL_PAGES / 8];
static unsigned long phys_mem_start = 0;

/**
 * @brief Inicializa el gestor de memoria física
 * @param start Dirección donde empieza la RAM libre (después del Kernel)
 */
void pmm_init(unsigned long start, unsigned long size) {
    phys_mem_start = start;

    /* Inicializamos enteramente a 0 (Libre) */
    memset(mem_map, 0, sizeof(mem_map));

    kprintf("[PMM v0.6] Gestionando %d MB de RAM física desde 0x%x (Demand Paging)\n",
            MEMORY_SIZE / (1024*1024), phys_mem_start);
}

/**
 * @brief Busca y reserva una página física libre
 * @return Dirección física de la página (o 0 si no hay memoria)
 * 
 * @details
 *   Implementa un algoritmo First-Fit sobre el bitmap:
 *   1. Busca el primer bit en 0 (página libre)
 *   2. Marca el bit como 1 (ocupado)
 *   3. Calcula la dirección física real
 *   4. Realiza security zeroing (limpia la página con 0s)
 *   
 *   INTEGRACIÓN CON DEMAND PAGING:
 *   Esta función es llamada por handle_fault() en sys.c cuando
 *   se produce un Page Fault y se necesita asignar memoria bajo demanda.
 */
unsigned long get_free_page(void) {
    /* Algoritmo First-Fit sobre Bitmap: Buscar el primer bit a 0 */
    for (int i = 0; i < TOTAL_PAGES; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;

        /* Comprobamos si el bit está libre (0) */
        if (! (mem_map[byte_index] & (1 << bit_index))) {
            /* ¡Encontrado! Lo marcamos como ocupado */
            mem_map[byte_index] |= (1 << bit_index);

            /* Calculamos la dirección física real */
            unsigned long page_addr = phys_mem_start + (i * PAGE_SIZE);

            /* Importante: Limpiamos la página (security zeroing) */
            memset((void*)page_addr, 0, PAGE_SIZE);

            return page_addr;
        }
    }

    kprintf("[PMM] CRITICAL: Out of Memory (OOM)!\n");
    return 0;
}

/**
 * @brief Libera una página física
 */
void free_page(unsigned long p) {
    unsigned long offset = p - phys_mem_start;
    int index = offset / PAGE_SIZE;

    if (index < 0 || index >= TOTAL_PAGES) return;

    int byte_index = index / 8;
    int bit_index = index % 8;

    /* Ponemos el bit a 0 */
    mem_map[byte_index] &= ~(1 << bit_index);
}
