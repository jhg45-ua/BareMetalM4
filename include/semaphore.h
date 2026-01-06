/**
 * @file semaphore.h
 * @brief Semaforos para sincronizacion entre procesos
 * 
 * Operaciones P() y V() de Dijkstra para proteger recursos compartidos.
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

/* Semaforo binario/contador para sincronizacion */
struct semaphore {
    volatile int count;  /* count > 0: disponible, count = 0: bloqueado */
};

/* Inicializa semaforo con valor inicial */
void sem_init(struct semaphore *s, int value);

/* Espera a que recurso este disponible (Operacion P) */
void sem_wait(struct semaphore *s);

/* Libera recurso y despierta proceso esperando (Operacion V) */
void sem_signal(struct semaphore *s);

#endif // SEMAPHORE_H