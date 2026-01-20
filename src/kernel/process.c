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
 * @version 0.3.5
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
    int pid = -1;

    /* 1. Buscar hueco libre (Reciclable) */
    /* Buscamos el primer slot que sea UNUSED (0) */
    for (int i = 0; i < MAX_PROCESS; i++) {
        if (process[i].state == PROCESS_UNUSED) {
            pid = i;
            break;
        }
    }

    /* Si no hay hueco, error */
    if (pid == -1) {
        kprintf("[KERNEL] Error: Tabla de procesos llena \n");
        return -1;
    }

    struct pcb *p = &process[pid];

    /* 2. Asignar memoria (Heap) */
    void *stack = kmalloc(4096);
    if (!stack) {
        kprintf("[KERNEL] Error: Out of Memory (PID %d)\n", pid);
        return -1;
    }

    /* Guardamos la direccion para que free_zombie pueda liberarla luego */
    p->stack_addr = (unsigned long)stack;

    /* 3. Configurar PCB */
    p->pid = pid;
    p->state = PROCESS_READY;
    p->priority = priority;
    p->prempt_count = 0;
    p->wake_up_time = 0;

    p->cpu_time = 0;
    p->block_reason = BLOCK_REASON_NONE;
    p->exit_code = 0;

    k_strncpy(p->name, name, 16);

    /* 4. Configurar contexto */
    p->context.x19 = (unsigned long)fn;
    p->context.pc = (unsigned long)ret_from_fork;
    p->context.sp = (unsigned long)stack + 4096;

    num_process++;

    return pid;
}

/**
 * @brief Crea un Hilo del Kernel (Kernel Thread)
 * * En BareMetalM4 (v0.3.5), como no hay separación de memoria virtual por proceso,
 * todos los procesos son técnicamente hilos del kernel que comparten espacio de direcciones.
 */
long create_thread(void (*fn)(void), int priority, const char *name) {
    /* Wrapper sobre create_process para cumplir con la semántica del Tema 2 */
    return create_process(fn, priority, name);
}

/**
 * @brief Inicializa el subsistema de gestión de procesos
 * 
 * Configura el proceso 0 (kernel/idle) y prepara las estructuras
 * para la creación de nuevos procesos.
 */
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

void free_zombie() {
    /* Recorremos toda la tabla de procesos */
    for (int i = 0; i < MAX_PROCESS; i++) {
        if (process[i].state == PROCESS_ZOMBIE) {
            // kprintf("[REAPER] Limpiando PID %d\n", process[i].pid); // Debug opcional

            /* 1. CRÍTICO: Devolver la memoria al Heap */
            if (process[i].stack_addr != 0) {
                kfree((void*)process[i].stack_addr);
                process[i].stack_addr = 0;
            }

            /* 2. Marcar el slot como reutilizable */
            process[i].state = PROCESS_UNUSED;

            /* Decrementamos contador */
            num_process--;
        }
    }
}