/**
 * @file semaphore.c
 * @brief Implementacion de semaforos para sincronizacion entre procesos
 * 
 * @details
 *   SEMAFOROS EN ESTE KERNEL:
 * 
 *   Implementacion "simplificada" del semaforo de Dijkstra.
 *   No usa colas de espera (wait queues como en kernels reales).
 *   En su lugar, usa busy-waiting con schedule() para ceder CPU.
 * 
 *   SEMANTICA CLASSICA (Dijkstra):
 *   @code
 *   P(s):  // Entrada a seccion critica (wait)
 *       while (s.count == 0) doNothing();  // Esperar
 *       s.count--;
 * 
 *   V(s):  // Salida de seccion critica (signal)
 *       s.count++;
 *   @endcode
 * 
 *   PROTECCION CONTRA RACE CONDITIONS:
 *   - spin_lock() adquiere un spinlock atomico (LDXR/STXR)
 *   - Solo un proceso puede modificar count simultaneamente
 * 
 *   BUSY-WAITING EN LUGAR DE WAIT QUEUES:
 *   - Esperar en un bucle (while) y ceder CPU con schedule()
 *   - Cuando otro proceso hace V() (signal), simplemente retorna
 *   - No hay mecanismo para "despertar" (wake) explicitamente
 *   
 *   VENTAJAS:
 *   - Implementacion MUY simple (3 funciones de 1-5 lineas)
 *   - Facil de entender para estudiantes
 * 
 *   DESVENTAJAS (comparado con un kernel real):
 *   - CPU se desperdicia revisando continuamente
 *   - Sin condition variables para coordinacion eficiente
 *   - Scaling pobre (muchos procesos esperando = perdida de tiempo)
 *   
 *   KERNELS REALES USAN:
 *   - Wait queues (procesos duermen, no consumen CPU)
 *   - Explicit wake-up (V() activa un waiter)
 *   - Eventos (condition variables como en pthreads)
 * 
 * @author Sistema Operativo Educativo
 * @version 0.4
 * @see semaphore.h para interfaz publica
 */

#include "../include/semaphore.h"

#include "../include/drivers/timer.h"
#include "../include/kernel/process.h"

extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);

/* Spinlock global para proteger operaciones de semaforo */
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
 * Decrementa el contador. Si es 0, espera activamente cediendo CPU.
 */
void sem_wait(struct semaphore *s) {
    /* 1. Proteger el acceso al semaforo */
    spin_lock(&sem_lock);

    if (s->count > 0) {
        /* Caso A: Semaforo libre. Pasamos directo */
        s->count--;
        spin_unlock(&sem_lock);
    } else {
        /* Caso B: Semaforo ocupado. Dormimos */
        /* Nos añadimos a la cola (Lista enlazada) */
        if (s->tail == nullptr) {
            s->head = current_process;
            s->tail = current_process;
        } else {
            s->tail->next = current_process; /* El ultimo apunta a nosotros */
            s->tail = current_process;       /* Ahora somos el último */
        }
        current_process->next = nullptr; /* Nosotros no apuntamos a nadie aún */

        /* Cambiamos estado a BLOQUEADO */
        current_process->state = PROCESS_BLOCKED;
        current_process->block_reason = BLOCK_REASON_WAIT;

        /* Liberamos el cerrojo ANTES de dormirnos (muy importante) */
        spin_unlock(&sem_lock);

        /* Cedemos la CPU. Como estamos BLOCKED, el scheduler no nos elegirá
           hasta que alguien nos despierte en sem_signal */
        schedule();

        enable_interrupts();
    }
}

/**
 * @brief Operación V (signal) - Libera el recurso
 * @param s Puntero al semáforo
 * 
 * Incrementa el contador, permitiendo que otros procesos accedan.
 */
void sem_signal(struct semaphore *s) {
    spin_lock(&sem_lock);

    if (s->head != nullptr) {
        /* Caso A: Hay alguien durmiendo. Lo despertamos */
        struct pcb *proceso_dormido = s->head;

        /* Avanzamos la cola (sacamos el primero) */
        s->head = proceso_dormido->next;
        if (s->head == nullptr) {
            s->tail = nullptr;
        }

        /* Lo despertamos: Ahora es elegible por el scheduler */
        proceso_dormido->state = PROCESS_READY;
        proceso_dormido->block_reason = BLOCK_REASON_NONE;
        proceso_dormido->next = nullptr;

        /* NOTA: No incrementamos s->count. Le pasamos el "turno"
           directamente al proceso que acabamos de despertar. */
    } else {
        /* CASO B: Nadie espera. Simplemente subimos el contador. */
        s->count++;
    }
    spin_unlock(&sem_lock);
}