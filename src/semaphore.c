/**
 * @file semaphore.c
 * @brief Implementación de semáforos con Wait Queues (NO busy-wait)
 * 
 * @details
 *   SEMÁFOROS CON WAIT QUEUES:
 * 
 *   Esta implementación usa wait queues reales, eliminando el busy-waiting
 *   de versiones anteriores. Los procesos bloqueados NO consumen CPU.
 * 
 *   SEMÁNTICA CLÁSICA (Dijkstra):
 *   @code
 *   P(s):  // Entrada a sección crítica (wait)
 *       if (s.count > 0) {
 *           s.count--;
 *       } else {
 *           block_process();       // Dormir en wait queue
 *           wait_for_signal();     // Despertar cuando otro haga V()
 *       }
 * 
 *   V(s):  // Salida de sección crítica (signal)
 *       if (wait_queue_not_empty) {
 *           wake_up_first_waiter(); // Despertar primer proceso en cola
 *       } else {
 *           s.count++;              // Nadie espera, incrementar contador
 *       }
 *   @endcode
 * 
 *   WAIT QUEUE (Cola de Espera):
 *   - Lista enlazada de procesos bloqueados (usando campo 'next' del PCB)
 *   - FIFO: Primero en bloquear, primero en despertar (fairness)
 *   - Procesos en estado BLOCKED no aparecen en schedule()
 * 
 *   PROTECCIÓN CONTRA RACE CONDITIONS:
 *   - spin_lock() adquiere un spinlock atómico (LDXR/STXR en ARM64)
 *   - Solo un proceso puede modificar el semáforo simultáneamente
 *   - Se libera el spinlock ANTES de dormir (evita deadlock)
 * 
 *   VENTAJAS vs BUSY-WAIT:
 *   - ✅ Procesos bloqueados NO consumen CPU
 *   - ✅ Despertar explícito (eficiente)
 *   - ✅ Escalabilidad: Muchos procesos esperando sin overhead
 *   - ✅ Similar a kernels reales (Linux, FreeBSD)
 * 
 *   DESVENTAJAS DEL BUSY-WAIT (versión antigua):
 *   - ❌ CPU se desperdicia revisando continuamente
 *   - ❌ Scaling pobre (muchos procesos = pérdida de tiempo)
 *   - ❌ Solo para sistemas simples/educativos
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
 * @see semaphore.h para interfaz pública
 */

#include "../include/semaphore.h"

#include "../include/drivers/timer.h"
#include "../include/kernel/process.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                         */
/* ========================================================================== */

/* Spinlocks atómicos implementados en ARM64 assembly (locks.S) */
extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);

/* Habilita interrupciones IRQ */
extern void enable_interrupts(void);

/* Spinlock global para proteger operaciones de semáforo */
volatile int sem_lock = 0;

/* ========================================================================== */
/* OPERACIONES DE SEMAFORO                                                   */
/* ========================================================================== */

/**
 * @brief Inicializa un semáforo con un valor inicial
 * @param s Puntero al semáforo
 * @param value Valor inicial del contador
 */
void sem_init(struct semaphore *s, int value) {
    s->count = value;
    s->head = nullptr;
    s->tail = nullptr;
}

/**
 * @brief Operación P (wait) - Espera hasta que el recurso esté disponible
 * @param s Puntero al semáforo
 * 
 * @details
 *   Implementación eficiente con wait queue:
 *   
 *   CASO A - Semáforo disponible (count > 0):
 *   1. Decrementa count
 *   2. Libera spinlock
 *   3. Retorna inmediatamente (acceso concedido)
 *   
 *   CASO B - Semáforo ocupado (count = 0):
 *   1. Añade el proceso a la wait queue (lista enlazada)
 *   2. Marca el proceso como BLOCKED (block_reason = BLOCK_REASON_WAIT)
 *   3. Libera el spinlock ANTES de dormir (crítico para evitar deadlock)
 *   4. Llama a schedule() - El proceso NO consume CPU mientras espera
 *   5. Cuando sem_signal() lo despierte, retorna (acceso concedido)
 *   
 *   ESTRUCTURA DE LA WAIT QUEUE:
 *   - Lista enlazada simple usando campo 'next' del PCB
 *   - head apunta al primero en esperar
 *   - tail apunta al último (para inserción O(1))
 *   - FIFO: Fairness - Primero en llegar, primero en ser atendido
 */
void sem_wait(struct semaphore *s) {
    /* 1. Adquirir spinlock para proteger el semáforo */
    spin_lock(&sem_lock);

    if (s->count > 0) {
        /* CASO A: Semáforo libre - Acceso concedido inmediatamente */
        s->count--;
        spin_unlock(&sem_lock);
    } else {
        /* CASO B: Semáforo ocupado - Bloquearse en wait queue */
        
        /* Añadir proceso actual a la cola de espera */
        if (s->tail == nullptr) {
            /* Cola vacía: Somos el primero */
            s->head = current_process;
            s->tail = current_process;
        } else {
            /* Cola no vacía: Nos añadimos al final */
            s->tail->next = current_process;
            s->tail = current_process;
        }
        current_process->next = nullptr;

        /* Cambiar estado a BLOQUEADO */
        current_process->state = PROCESS_BLOCKED;
        current_process->block_reason = BLOCK_REASON_WAIT;

        /* CRÍTICO: Liberar el spinlock ANTES de dormir
           Si no liberamos aquí, nadie más puede hacer sem_signal() -> deadlock */
        spin_unlock(&sem_lock);

        /* Ceder la CPU - El scheduler nos ignorará hasta que nos despierten
           NO consumimos CPU mientras esperamos (NO busy-wait) */
        schedule();

        /* Al despertar, habilitamos interrupciones */
        enable_interrupts();
    }
}

/**
 * @brief Operación V (signal) - Libera el recurso
 * @param s Puntero al semáforo
 * 
 * @details
 *   Implementación eficiente con wake-up explícito:
 *   
 *   CASO A - Hay procesos esperando (wait queue no vacía):
 *   1. Extrae el primer proceso de la cola (FIFO)
 *   2. Lo marca como READY (block_reason = NONE)
 *   3. El proceso NO incrementa count (pasa el turno directamente)
 *   4. En el próximo schedule(), el proceso puede ejecutar
 *   
 *   CASO B - Nadie espera (wait queue vacía):
 *   1. Incrementa count (el recurso queda disponible)
 *   2. El próximo sem_wait() puede pasar directamente
 *   
 *   NOTA IMPORTANTE:
 *   - NO incrementamos count cuando despertamos a alguien
 *   - Le "pasamos el turno" directamente al proceso despertado
 *   - Esto evita race conditions: El despertado tiene el recurso garantizado
 */
void sem_signal(struct semaphore *s) {
    spin_lock(&sem_lock);

    if (s->head != nullptr) {
        /* CASO A: Hay procesos esperando - Despertar el primero */
        
        /* Extraer el primer proceso de la cola */
        struct pcb *proceso_dormido = s->head;

        /* Avanzar la cola (FIFO: Sacamos el primero) */
        s->head = proceso_dormido->next;
        if (s->head == nullptr) {
            /* Era el último, la cola queda vacía */
            s->tail = nullptr;
        }

        /* Despertar el proceso: Cambiar a READY */
        proceso_dormido->state = PROCESS_READY;
        proceso_dormido->block_reason = BLOCK_REASON_NONE;
        proceso_dormido->next = nullptr;

        /* NOTA IMPORTANTE: NO incrementamos s->count
           Le pasamos el "turno" directamente al proceso despertado
           Esto evita race conditions: El proceso tiene acceso garantizado */
    } else {
        /* CASO B: Nadie espera - Simplemente incrementar contador */
        s->count++;
    }
    
    spin_unlock(&sem_lock);
}