/**
 * @file scheduler.c
 * @brief Planificador de procesos con prioridades y aging
 * 
 * @details
 *   Implementa el scheduler del sistema operativo:
 *   - Algoritmo de prioridades con aging
 *   - Sistema de sleep/wake para procesos
 *   - Manejo de timer ticks
 * 
 * @section ALGORITMO_SCHEDULER
 *   El scheduler implementa un algoritmo de prioridades con "aging":
 *   1. ENVEJECIMIENTO: Para procesos esperando, priority--
 *   2. SELECCION: Buscar el READY con MENOR priority
 *   3. PENALIZACION: Aumentar priority del seleccionado para evitar monopolio
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#include "../../include/sched.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                         */
/* ========================================================================== */

/* Context switch entre dos procesos (src/entry.S) */
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);

extern void enable_interrupts(void);

volatile int need_reschedule = 0;

int is_reschedule_pending () {
    return need_reschedule;
}

/* ========================================================================== */
/* SCHEDULER - ALGORITMO DE PRIORIDADES CON AGING                           */
/* ========================================================================== */

/**
 * @brief Planificador de procesos (Scheduler)
 * 
 * Implementa un algoritmo de prioridades con aging para evitar starvation.
 * El proceso con menor valor de priority tiene mayor prioridad de ejecución.
 */
void schedule(void) {
    need_reschedule = 0;

    /* 1. Fase de Envejecimiento */
    for (int i = 0; i < MAX_PROCESS; i++) {
        if (process[i].state == PROCESS_READY && i != current_process->pid) {
            if (process[i].priority > 0) {
                process[i].priority--;
            }
        }
    }

    /* 2. Fase de Elección */
    int next_pid = -1;
    int highest_priority_found = 1000;

    for (int i = 0; i < MAX_PROCESS; i++) {
        if (process[i].state == PROCESS_READY || process[i].state == PROCESS_RUNNING) {
            if (process[i].priority < highest_priority_found) {
                highest_priority_found = process[i].priority;
                next_pid = i;
            }
        }
    }

    /* Si nadie quiere la CPU (todos dormidos o muertos)... */
    if (next_pid == -1) {
        /* ... se la damos al Kernel (PID 0) por defecto */
        next_pid = 0;

        /* SALVAVIDAS: Si el Kernel estaba bloqueado por error, lo despertamos */
        if (process[0].state != PROCESS_RUNNING && process[0].state != PROCESS_READY) {
            process[0].state = PROCESS_READY;
        }
    }

    struct pcb *next = &process[next_pid];
    struct pcb *prev = current_process;

    /* Penalizar tarea seleccionada para evitar monopolio */
    if (next->priority < 10) {
        next->priority += 2;
    }

    if (next->pid > 0) {
        next->quantum = DEFAULT_QUANTUM;
    }

    if (prev != next) {
        if (prev->state == PROCESS_RUNNING) prev->state = PROCESS_READY;
        next->state = PROCESS_RUNNING;
        current_process = next;

        cpu_switch_to(prev, next);
    }
}

/* ========================================================================== */
/* TIMER TICK: ACTUALIZACION DEL SISTEMA DE TIEMPO Y DESPERTAR PROCESOS      */
/* ========================================================================== */

/* Contador global de ticks del sistema */
volatile unsigned long sys_timer_count = 0;

/**
 * @brief Manejador de ticks del timer
 * 
 * Se ejecuta en cada interrupción del timer para:
 * - Incrementar el contador de ticks
 * - Despertar procesos bloqueados cuando corresponde
 */
void timer_tick(void) {
    sys_timer_count++;

    if (current_process->state == PROCESS_RUNNING) {
        current_process->cpu_time++;

        /* Logica Round-Robin */
        /* Si no es el kernel (PID 0), le restamos vida */
        if (current_process->pid > 0) {
            current_process->quantum--;

            /* Se acabó el tiempo para el proceso */
            if (current_process->quantum <= 0) {
                // kprintf("[SCHED] PID %d agoto su quantum. Expropiando...\n", current_process->pid);

                /* Forzamos el cambio de contexto AHORA MISMO.
                   Como estamos dentro de una interrupción (IRQ),
                   al volver de schedule() seguiremos en el handler
                   y luego haremos kernel_exit normalmente. */
                // schedule();
                /* CAMBIO: NO llamamos a schedule(). Solo levantamos la mano. */
                need_reschedule = 1;
            }
        }
    }

    for (int i = 0; i < MAX_PROCESS; i++) {
        if (process[i].state == PROCESS_BLOCKED) {
            if (process[i].block_reason == BLOCK_REASON_SLEEP) {
                if (process[i].wake_up_time <= sys_timer_count) {
                    process[i].state = PROCESS_READY;
                    process[i].block_reason = BLOCK_REASON_NONE;
                    // kprintf(" [KERNEL] Despertando proceso %d en tick %d\n", i, sys_timer_count);    // DEBUG
                }
            }
        }
    }
}

/* ========================================================================== */
/* SLEEP: DORMIR UN PROCESO POR TIEMPO DETERMINADO                         */
/* ========================================================================== */

/**
 * @brief Duerme el proceso actual durante un número de ticks del timer
 * @param ticks Número de ticks del timer a dormir
 */
void sleep(unsigned int ticks) {
    current_process->wake_up_time = sys_timer_count + ticks;
    current_process->state = PROCESS_BLOCKED;
    current_process->block_reason = BLOCK_REASON_SLEEP;
    schedule();

    enable_interrupts();
}
