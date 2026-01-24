/**
 * @file semaphore.h
 * @brief Semáforos con Wait Queues para sincronización eficiente
 * 
 * @details
 *   Implementación de semáforos de Dijkstra con wait queues reales.
 *   Los procesos bloqueados NO consumen CPU (NO busy-wait).
 *   
 *   CARACTERÍSTICAS:
 *   - Wait Queue: Lista enlazada FIFO de procesos bloqueados
 *   - Bloqueo eficiente: Procesos BLOCKED no aparecen en schedule()
 *   - Wake-up explícito: sem_signal() despierta al primer waiter
 *   - Spinlocks: Protección atómica contra race conditions
 *   
 *   USO TÍPICO:
 *   @code
 *   struct semaphore mutex;
 *   sem_init(&mutex, 1);  // Semáforo binario (mutex)
 *   
 *   // Sección crítica
 *   sem_wait(&mutex);     // Bloquea si otro proceso lo tiene
 *   // ... código protegido ...
 *   sem_signal(&mutex);   // Libera y despierta al siguiente
 *   @endcode
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "sched.h"

/* ========================================================================== */
/* ESTRUCTURAS                                                               */
/* ========================================================================== */

/**
 * @brief Estructura de semáforo con wait queue
 * 
 * @details
 *   count: Contador del semáforo
 *     - count > 0: Recurso disponible, sem_wait() pasa inmediatamente
 *     - count = 0: Recurso ocupado, sem_wait() bloquea el proceso
 *   
 *   head/tail: Wait queue (cola de espera)
 *     - Lista enlazada de procesos bloqueados (campo 'next' del PCB)
 *     - FIFO: Fairness - Primero en llegar, primero en despertar
 *     - Si ambos son NULL, la cola está vacía
 */
struct semaphore {
    volatile int count;  /* Contador: >0=disponible, 0=ocupado */
    struct pcb *head;    /* Primer proceso en wait queue */
    struct pcb *tail;    /* Último proceso en wait queue */
};

/* ========================================================================== */
/* FUNCIONES PUBLICAS                                                        */
/* ========================================================================== */

/**
 * @brief Inicializa semáforo con valor inicial
 * @param s Puntero al semáforo
 * @param value Valor inicial del contador (ej: 1 para mutex, N para recursos)
 * 
 * @details
 *   Inicializa el contador y la wait queue vacía.
 *   Debe llamarse antes de usar el semáforo.
 */
void sem_init(struct semaphore *s, int value);

/**
 * @brief Espera a que recurso esté disponible (Operación P / wait)
 * @param s Puntero al semáforo
 * 
 * @details
 *   Si el recurso está disponible (count > 0):
 *   - Decrementa count y retorna inmediatamente
 *   
 *   Si el recurso está ocupado (count = 0):
 *   - Añade el proceso a la wait queue
 *   - Marca el proceso como BLOCKED (block_reason = BLOCK_REASON_WAIT)
 *   - NO consume CPU mientras espera (NO busy-wait)
 *   - Despertará cuando sem_signal() lo active
 */
void sem_wait(struct semaphore *s);

/**
 * @brief Libera recurso y despierta proceso esperando (Operación V / signal)
 * @param s Puntero al semáforo
 * 
 * @details
 *   Si hay procesos esperando en la wait queue:
 *   - Despierta al primer proceso (FIFO)
 *   - Lo marca como READY para que schedule() lo elija
 *   - NO incrementa count (pasa el turno directamente)
 *   
 *   Si nadie espera:
 *   - Incrementa count (recurso queda disponible)
 */
void sem_signal(struct semaphore *s);

#endif // SEMAPHORE_H