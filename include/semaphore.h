#ifndef SEMAPHORE_H
#define SEMAPHORE_H

struct semaphore {
    volatile int count;
    /*
        En un SO real, aqui iria una lista de espera(wait_queue). Para simplificar, marcaremos procesos como BLOCKED y
        el scheduler los saltara hasta que el semaforo los despierte
    */
};

void sem_init(struct semaphore *s, int value);
void sem_wait(struct semaphore *s);
void sem_signal(struct semaphore *s);

#endif