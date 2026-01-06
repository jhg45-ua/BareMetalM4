/**
 * @file process.c
 * @brief Gestión de procesos del kernel
 * 
 * @details
 *   Implementa la gestión de procesos:
 *   - Creación de threads del kernel
 *   - Terminación de procesos
 *   - Process Control Blocks (PCB)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

#include "../../include/types.h"
#include "../../include/sched.h"
#include "../../include/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"

/* ========================================================================== */
/* GESTION DE PROCESOS - ESTRUCTURAS GLOBALES                               */
/* ========================================================================== */

/* Array de Process Control Blocks para todos los procesos */
struct pcb process[MAX_PROCESS];

/* Puntero al proceso actualmente en ejecución */
struct pcb *current_process;

/* Contador del número total de procesos creados */
int num_process = 0;

/* Stacks de ejecución para cada proceso (256KB total) */
uint8_t process_stack[MAX_PROCESS][4096] __attribute__((aligned(16)));

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                         */
/* ========================================================================== */

/* Habilita las interrupciones IRQ en el procesador */
extern void enable_interrupts(void);

/* Punto de entrada para nuevos procesos (src/entry.S) */
extern void ret_from_fork(void);

/* ========================================================================== */
/* TERMINACION DE PROCESOS                                                   */
/* ========================================================================== */

/**
 * @brief Termina el proceso actual y cede el CPU
 */
void exit(void) {
    enable_interrupts();

    kprintf("\n[KERNEL] Proceso %d (%d) ha terminado. Muriendo...\n", 
            current_process->pid, current_process->priority);

    /* Marcar como zombie (PCB persiste hasta implementar wait/reaper) */
    current_process->state = PROCESS_ZOMBIE;

    /* Ceder CPU (schedule() nunca nos volverá a elegir) */
    schedule();
}

/**
 * @brief Hook post-fork (reservado para futuras extensiones)
 */
void schedule_tail(void) {
    /* Placeholder para lógica post-context-switch */
}

/* ========================================================================== */
/* CREACION DE PROCESOS                                                      */
/* ========================================================================== */

/**
 * @brief Crea un nuevo thread (hilo) del kernel
 * @param fn Función a ejecutar
 * @param priority Prioridad inicial del proceso
 * @param name Nombre descriptivo del proceso (máx 15 chars)
 * @return PID del proceso creado, -1 en caso de error
 */
int create_thread(void (*fn)(void), int priority, const char *name) {
    if (num_process >= MAX_PROCESS) return -1;
    
    struct pcb *p = &process[num_process];

    p->pid = num_process;
    p->state = PROCESS_READY;
    p->prempt_count = 0;
    p->priority = priority;
    p->wake_up_time = 0;

    k_strncpy(p->name, name, 16);

    /* Guardamos la función en x19 */
    p->context.x19 = (unsigned long)fn;
    /* El PC apunta al wrapper */
    p->context.pc = (unsigned long)ret_from_fork;
    /* La pila crece hacia abajo, apuntamos al final del bloque de 4KB */
    p->context.sp = (unsigned long)&process_stack[num_process][4096];

    num_process++;
    return p->pid;
}
