#include "../include/semaphore.h"
#include "../include/sched.h"

extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);
extern void schedule(void);

/*
    Como no tenemos listas de espera complejas, usaremos un spinlock global para proteger, la operacion
    del semaforo atomicamente
*/
volatile int sem_lock = 0;


void sem_init(struct semaphore *s, int value) {
    s->count = value;
}

/* WAIT [P()]: Intenta entrar, si no puede se duerme */
void sem_wait(struct semaphore *s) {
    while (1) {
        spin_lock(&sem_lock); // Protegemos el acceso

        if (s->count > 0) {
            s->count--;
            spin_unlock(&sem_lock);
            return; // Entramos con exito
        }
        /*
            El recurso esta ocupado: BLOQUEAMOS el proceso actual
            Nota: Esto es insuficiente porque soltariamos el lock y volvemos a intentarlo
            en la siguiente vuelta, pero ilustra el concepto de "no pasar"
        */
        spin_unlock(&sem_lock);

        /* 
            Marcamos el proceso como BLOQUEADO para que el scheduler no lo elija
            NOTA IMPORTANTE: En esta implementación simplificada, necesitamos
            que el sem_post nos despierte.
        */
        
        // Simulación simple de bloqueo: cedemos CPU voluntariamente
        // En un sistema completo: current->state = TASK_BLOCKED;
        schedule();
    }

    
}

/* SIGNAL [V()]: Libera y despierta */
void sem_signal(struct semaphore *s) {
    spin_lock(&sem_lock);
    s->count++;
    spin_unlock(&sem_lock);
    /*
        En un sistema real, aquí buscaríamos un proceso BLOCKED en la lista
        del semáforo y lo pondríamos en READY
    */
}