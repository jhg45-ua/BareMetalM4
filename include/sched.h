/**
 * @file sched.h
 * @brief Interfaz para planificación de procesos
 * 
 * @details
 *   Define estructuras y constantes para el manejo de procesos:
 *   
 *   ESTADOS DE PROCESO:
 *   - Máquina de estados completa (UNUSED → READY → RUNNING → ZOMBIE)
 *   - Estados de bloqueo (BLOCKED) con razones específicas
 *   
 *   PROCESS CONTROL BLOCK (PCB):
 *   - Contexto de CPU para context switch (x19-x28, fp, pc, sp)
 *   - Información de scheduling (priority, quantum, wake_up_time)
 *   - Campo 'next' para wait queues en semáforos
 *   
 *   CARACTERÍSTICAS DE SCHEDULING:
 *   - Round-Robin con Quantum configurable (DEFAULT_QUANTUM)
 *   - Prioridades con aging
 *   - Wait queues para sincronización eficiente
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef SCHED_H
#define SCHED_H

/* ========================================================================== */
/* ESTADOS DE PROCESO                                                        */
/* ========================================================================== */

/**
 * @brief Estados del ciclo de vida de un proceso
 * 
 * UNUSED (0): Slot libre, puede ser reutilizado
 * RUNNING (1): Proceso actualmente ejecutándose en la CPU
 * READY (2): Proceso listo para ejecutar, esperando su turno
 * BLOCKED (3): Proceso bloqueado (durmiendo o esperando recurso)
 * ZOMBIE (4): Proceso terminado, esperando limpieza (free_zombie)
 */
#define PROCESS_UNUSED 0
#define PROCESS_RUNNING 1
#define PROCESS_READY 2
#define PROCESS_BLOCKED 3
#define PROCESS_ZOMBIE 4

/* ========================================================================== */
/* RAZONES DE BLOQUEO                                                        */
/* ========================================================================== */

/**
 * @brief Razones por las que un proceso puede estar BLOCKED
 * 
 * BLOCK_REASON_NONE (0): No bloqueado (estado READY/RUNNING)
 * BLOCK_REASON_SLEEP (1): Durmiendo por sleep() - Despierta en timer_tick()
 * BLOCK_REASON_WAIT (2): Esperando semáforo/recurso - Despierta en sem_signal()
 * 
 * IMPORTANTE: Los procesos BLOCKED NO consumen CPU.
 * El scheduler (schedule()) los ignora hasta que sean despertados.
 */
#define BLOCK_REASON_NONE 0
#define BLOCK_REASON_SLEEP 1
#define BLOCK_REASON_WAIT 2     /* Semáforos, I/O, etc. */

/* ========================================================================== */
/* CONSTANTES DEL SISTEMA                                                    */
/* ========================================================================== */

/**
 * @brief Constantes de configuración del sistema
 * 
 * MAX_PROCESS (64): Número máximo de procesos simultáneos
 * BUFFER_SIZE (4): Tamaño de buffers internos
 * DEFAULT_QUANTUM (5): Quantum de Round-Robin en ticks
 *   - Cada proceso recibe 5 ticks de CPU antes de ser expropiado
 *   - Se decrementa en timer_tick()
 *   - Cuando llega a 0, se marca need_reschedule
 *   - Balance entre responsividad y overhead de context switch
 */
#define MAX_PROCESS 64
#define BUFFER_SIZE 4
#define DEFAULT_QUANTUM 5  /* Ticks de quantum para Round-Robin */

/* ========================================================================== */
/* ESTRUCTURAS                                                               */
/* ========================================================================== */

/**
 * @brief Registros ARM64 guardados durante context switch
 * 
 * @details
 *   Contexto de CPU que se guarda/restaura en cpu_switch_to():
 *   - x19-x28: Registros callee-saved (según AAPCS64)
 *   - fp (x29): Frame Pointer
 *   - pc: Program Counter (dirección de retorno)
 *   - sp: Stack Pointer
 *   
 *   Los registros x0-x18 son caller-saved y no se guardan aquí.
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
    unsigned long fp;   /* x29 - Frame Pointer */
    unsigned long pc;   /* Program Counter */
    unsigned long sp;   /* Stack Pointer */
};

/**
 * @brief Process Control Block (PCB) - Información completa de un proceso
 * 
 * @details
 *   Estructura central del scheduler. Contiene:
 *   
 *   CONTEXTO Y ESTADO:
 *   - context: Registros CPU guardados para context switch
 *   - state: Estado actual (UNUSED/RUNNING/READY/BLOCKED/ZOMBIE)
 *   - pid: Process ID único
 *   
 *   SCHEDULING:
 *   - priority: Valor de prioridad (menor valor = mayor prioridad)
 *   - quantum: Ticks restantes antes de expropriación (Round-Robin)
 *   - wake_up_time: Tick en el que despertar si block_reason=SLEEP
 *   
 *   BLOQUEO Y SINCRONIZACIÓN:
 *   - block_reason: Por qué está bloqueado (SLEEP, WAIT, NONE)
 *   - next: Puntero para wait queues en semáforos (lista enlazada)
 *   
 *   MEMORIA:
 *   - stack_addr: Dirección base del stack (para kfree en free_zombie)
 *   
 *   ESTADÍSTICAS:
 *   - cpu_time: Ticks de CPU consumidos (para profiling)
 *   - exit_code: Valor de retorno al terminar
 *   - name: Nombre descriptivo (debugging)
 */
struct pcb {
    struct cpu_context context;  /* Contexto de CPU (context switch) */
    long state;                  /* Estado del proceso */
    long pid;                    /* Process ID */
    int priority;                /* Prioridad (menor = más urgente) */
    long prempt_count;           /* Contador de preempciones */
    unsigned long wake_up_time;  /* Tick para despertar (si BLOCKED) */
    char name[16];               /* Nombre del proceso (debug) */
    unsigned long stack_addr;    /* Dirección base de la pila */

    unsigned long cpu_time;      /* Tiempo de CPU consumido (ticks) */
    int block_reason;            /* Razón de bloqueo (SLEEP/WAIT/NONE) */
    int exit_code;               /* Código de salida */

    int quantum;                 /* Quantum restante (Round-Robin) */
    struct pcb *next;            /* Para wait queues en semáforos */
};

#endif // SCHED_H