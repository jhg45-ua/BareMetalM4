/**
 * @file malloc.c
 * @brief Gestor de heap sencillo (First Fit)
 *
 * @details
 *   Implementa un asignador dinámico minimalista para el kernel:
 *   - Lista enlazada simple de bloques (first-fit)
 *   - Alineación a 16 bytes (ARM64)
 *   - Sin coalescencia (pendiente de mejora)
 *
 *   Diseñado para fines educativos: código claro y corto.
 */

#include "../../include/mm/malloc.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/kutils.h"

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

/**
 * @brief Inicializa el heap del kernel
 * @param start_addr Dirección inicial del heap
 * @param end_addr   Dirección final (exclusiva) del heap
 *
 * Alinea el inicio a 16 bytes y crea un bloque libre único
 * que cubre todo el rango disponible.
 */
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

/**
 * @brief Reserva memoria del heap (first-fit)
 * @param size Tamaño solicitado en bytes
 * @return Puntero a la región asignada o nullptr si no hay espacio
 */
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

            /* 4. Calcular puntero, LIMPIAR y devolver */
            void *ptr = (void *)(curr + 1); // Puntero a los datos

            /* Limpiamos 'size' bytes con 0 */
            memset(ptr, 0, size);

            /* 4. Devolver puntero a LOS DATOS (justo después del header) */
            return ptr;
        }
        curr = curr->next;
    }
    kprintf("[HEAP] Error: Out of Memory!\n");
    return nullptr; // NULL
}

/**
 * @brief Libera un bloque previamente asignado
 * @param ptr Puntero devuelto por kmalloc
 *
 * Marca el bloque como libre. No realiza coalescencia aún.
 */
void kfree(void *ptr) {
    if (!ptr) return;

    /* Obtenemos el header del bloque a liberar */
    struct block_header *curr = (struct block_header *)ptr - 1;

    curr->is_free = 1;

    /* Fusion (Coalescing) */
    /* Miramos hacia adelante: El siguiente bloque existe y es libre?? */
    while (curr->next != 0 && curr->next->is_free) {
        /* Sumamos el tamaño del siguiente bloque + su cabecera actual */
        curr->size += curr->next->size + sizeof(struct block_header);

        /* Saltamos el siguiente bloque  (lo engullimos) */
        curr->next = curr->next->next;
    }
    /* Nota: Para un coalescing perfecto necesitaríamos una lista doblemente
       enlazada para mirar también hacia ATRÁS (prev), pero esto ya mejora un 80% */
}
