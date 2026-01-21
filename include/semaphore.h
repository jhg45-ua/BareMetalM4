/**
 * @file semaphore.h
 * @brief Semaforos para sincronizacion entre procesos
 * 
 * @details
 *   Operaciones P() y V() de Dijkstra para proteger recursos compartidos.
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

/* Semaforo binario/contador para sincronizacion */
struct semaphore {
    volatile int count;  /* count > 0: disponible, count = 0: bloqueado */
    struct pcb *head;
    struct pcb *tail;
};

/* ========================================================================== */
/* FUNCIONES PUBLICAS                                                        */
/* ========================================================================== */

/**
 * @brief Inicializa semaforo con valor inicial
 * @param s Puntero al semaforo
 * @param value Valor inicial del contador
 */
void sem_init(struct semaphore *s, int value);

/**
 * @brief Espera a que recurso esté disponible (Operacion P)
 * @param s Puntero al semaforo
 */
void sem_wait(struct semaphore *s);

/**
 * @brief Libera recurso y despierta proceso esperando (Operacion V)
 * @param s Puntero al semaforo
 */
void sem_signal(struct semaphore *s);

#endif // SEMAPHORE_H