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

extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);
extern void schedule(void);

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
}

/**
 * @brief Operación P (wait) - Espera hasta que el recurso esté disponible
 * @param s Puntero al semáforo
 * 
 * Decrementa el contador. Si es 0, espera activamente cediendo CPU.
 */
void sem_wait(struct semaphore *s) {
    while (1) {
        spin_lock(&sem_lock);

        if (s->count > 0) {
            s->count--;
            spin_unlock(&sem_lock);
            return;
        }

        spin_unlock(&sem_lock);
        schedule();
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
    s->count++;
    spin_unlock(&sem_lock);
}