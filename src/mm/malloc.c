#include "../../include/mm/malloc.h"
#include "../../include/drivers/io.h"

// Algoritmo: Lista Enlazada Simple (First Fit)

/* Cabecera de los bloques de datos */
struct block_header {
    unsigned long size;     // Tamaño de DATOS (sin contar el header)
    struct block_header *next; // Puntero al siguiente bloque
    uint8_t is_free;            // 1 = Libre; 0 = Ocupado
    uint8_t padding[7];         // Relleno para alinear a 16 bytes (importante en ARM64)
};

// Inicio de la lista
static struct block_header *head = nullptr;

// Inicializar al Heap
void kheap_init(unsigned long start_addr, unsigned long end_addr) {
    // Ajustamos el inicio para que esté alineado a 16 bytes
    if (start_addr % 16 != 0) {
        start_addr = start_addr + (16 - (start_addr % 16));
    }

    head = (struct block_header *)start_addr;

    // El primer bloque ocupa TODA la RAM disponible
    // Tamaño total - Tamaño del header
    head->size = (end_addr - start_addr) - sizeof(struct block_header);
    head->next = nullptr; // No hay siguiente
    head->is_free = 1; // Está libre

    kprintf("   [HEAD] Iniciando en 0x%x. Tamaño inicial: %d bytes\n", start_addr, head->size);
}

void *kmalloc(uint32_t size) {
    struct block_header *curr = head;

    // Alineamos 16 bytes
    if (size % 16 != 0) {
        size = size + (16 - (size % 16));
    }

    /* 1. Recorrer la lista buscando un hueco */
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            /* Encontrado */

            /* 2. Podemos dividir el bloque? (Split) */
            /* Solo dividimos si sobra espacio para otro header + algo de datos (ej. 16 bytes) */
            if (curr->size > size + sizeof(struct block_header) + 16) {
                /* Calculamos donde empieza el nuevo bloque sobrante */
                /* Direccion actual + Header + Tamaño que nos han pedido */
                struct block_header *new_block = (struct block_header *)(
                    (char *)curr + sizeof(struct block_header) + size
                );

                /* Configurar el nuevo bloque (el sobrante) */
                new_block->size = curr->size - size - sizeof(struct block_header);
                new_block->is_free = 1;
                new_block->next = curr->next;

                /* Ajustar el bloque actual (el que vamos a entregar) */
                curr->size = size;
                curr->next = new_block;
            }

            /* 3. Marcar como ocupado */
            curr->is_free = 0;

            /* 4. Devolver puntero a LOS DATOS (justo después del header) */
            return (void *)(curr + 1);
        }
        curr = curr->next;
    }
    kprintf("[HEAP] Error: Out of Memory!\n");
    return nullptr; // NULL
}

void kfree(void *ptr) {
    if (!ptr) return;

    /* El puntero apunta a los datos. El header está justo antes. */
    struct block_header *header = (struct block_header *)ptr - 1;
    
    header->is_free = 1;
    
    /* (Mejora futura: Aquí deberíamos "fusionar" (coalesce) 
       bloques libres contiguos para evitar fragmentación) */
}
