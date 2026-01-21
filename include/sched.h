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
#define PROCESS_UNUSED 0
#define PROCESS_RUNNING 1
#define PROCESS_READY 2
#define PROCESS_BLOCKED 3
#define PROCESS_ZOMBIE 4

/* Razones de bloqueo (Mejora visual) */
#define BLOCK_REASON_NONE 0
#define BLOCK_REASON_SLEEP 1
#define BLOCK_REASON_WAIT 2     /* Semaforos I/O */

/* Constantes del sistema */
#define MAX_PROCESS 64
#define BUFFER_SIZE 4
#define DEFAULT_QUANTUM 5

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
    int priority;
    long prempt_count;
    unsigned long wake_up_time;
    char name[16];              /* Nombre del proceso */
    unsigned long stack_addr;   /* Dirección base de la pila asignada */

    unsigned long cpu_time;     /* Contador de tiempo de cpu */
    int block_reason;           /* Razon de bloqueo */
    int exit_code;              /* Valor de retorno al morir */

    int quantum;
    struct pcb *next;
};

#endif // SCHED_H