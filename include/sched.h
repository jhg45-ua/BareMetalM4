/**
 * @file sched.h
 * @brief Interfaz para planificación de procesos
 * 
 * Define estructuras y constantes para el manejo de procesos
 * en el sistema operativo.
 */

#ifndef SCHED_H
#define SCHED_H

/* Estados de proceso */
#define PROCESS_RUNNING 0
#define PROCESS_READY 1
#define PROCESS_BLOCKED 2
#define PROCESS_ZOMBIE 3

/* Constantes del sistema */
#define MAX_PROCESS 64
#define BUFFER_SIZE 4

/**
 * @brief Registros ARM64 guardados durante context switch
 */
struct cpu_context {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;
    unsigned long pc;
    unsigned long sp;
};

/**
 * @brief Process Control Block - información de cada proceso
 */
struct pcb {
    struct cpu_context context;
    long state;
    long pid;
    long prempt_count;
    int priority;
    unsigned long wake_up_time;
    unsigned long stack_addr; /* Dirección base de la pila asignada */
    char name[16]; /* Nombre del proceso */
};

#endif // SCHED_H