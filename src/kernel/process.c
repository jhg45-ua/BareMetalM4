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

#include "../../include/sched.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"
#include "../../include/mm/malloc.h"

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
// uint8_t process_stack[MAX_PROCESS][4096] __attribute__((aligned(16)));

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                           */
/* ========================================================================== */

/* Habilita las interrupciones IRQ en el procesador */
extern void enable_interrupts(void);

/* Punto de entrada para nuevos procesos (src/entry.S) */
extern void ret_from_fork(void);

/* ========================================================================== */
/* CREACION DE PROCESOS                                                      */
/* ========================================================================== */

/**
 * @brief Crea un nuevo thread (hilo) del kernel
 * @param fn Función a ejecutar
 * @param priority Prioridad inicial del proceso
 * @param name Nombre descriptivo del proceso (máx. 15 chars)
 * @return PID del proceso creado, -1 en caso de error
 */
long create_process(void (*fn)(void), int priority, const char *name) {
    if (num_process >= MAX_PROCESS) return -1;
    
    struct pcb *p = &process[num_process];

    /* 1. Asignacion Dinamica de la pila */
    /* Pedimos 4KB (4096) al HEAP */
    void *stack = kmalloc(4096);

    if (!stack) return -1;

    p->stack_addr = (unsigned long)stack;

    /* 2. Configurar PCB */
    p->pid = num_process;
    p->state = PROCESS_READY;
    p->prempt_count = 0;
    p->priority = priority;
    p->wake_up_time = 0;

    k_strncpy(p->name, name, 16);

    /* 3. Configurar Contexto */
    /* Guardamos la función en x19 */
    p->context.x19 = (unsigned long)fn;
    /* El PC apunta al wrapper */
    p->context.pc = (unsigned long)ret_from_fork;
    /* La pila crece hacia abajo, apuntamos al final del bloque de 4KB */
    p->context.sp = (unsigned long)stack + 4096;

    num_process++;
    return p->pid;
}

void init_process_system() {
    /* Limpiamos la tabla de procesos (opcional si está en .bss, pero seguro) */
    num_process = 0;

    /* Configurar Proceso 0 (IDLE/KERNEL) */
    /* Este proceso es especial: ya está ejecutándose y usa la pila de arranque */
    struct pcb *kproc = &process[0];

    kproc->pid = 0;
    kproc->state = PROCESS_RUNNING;
    kproc->priority = 0;
    kproc->stack_addr = 0;
    kproc->prempt_count = 0;

    k_strncpy(kproc->name, "Kernel", 16);

    /* Apuntamos el puntero global al proceso 0 */
    current_process = kproc;
    num_process = 1;

    kprintf("   [PROC] Subsistema de procesos iniciado. PID 0 activo.\n");
}


/* ========================================================================== */
/* TERMINACION DE PROCESOS                                                    */
/* ========================================================================== */

/**
 * @brief Termina el proceso actual y cede la CPU
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