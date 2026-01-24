/**
 * @file process.c
 * @brief Gestión de procesos del kernel
 * 
 * @details
 *   Implementa la gestión de procesos:
 *   - Creación de threads del kernel
 *   - Terminación de procesos
 *   - Process Control Blocks (PCB) con soporte para:
 *     * Quantum (Round-Robin scheduling)
 *     * Campos next para wait queues (semáforos)
 *     * wake_up_time para sleep() sin busy-wait
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
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

extern void move_to_user_mode(unsigned long pc, unsigned long sp);

/* ========================================================================== */
/* CREACION DE PROCESOS                                                      */
/* ========================================================================== */

/**
 * @brief Crea un nuevo thread (hilo) del kernel
 * @param fn Función a ejecutar
 * @param arg Argumento para la función
 * @param priority Prioridad inicial del proceso
 * @param name Nombre descriptivo del proceso (máx. 15 chars)
 * @return PID del proceso creado, -1 en caso de error
 * 
 * @details
 *   Flujo de creación:
 *   1. Buscar slot UNUSED en process[] (reciclable)
 *   2. Asignar stack de 4KB mediante kmalloc()
 *   3. Configurar PCB:
 *      - quantum: Se inicializará en schedule() al ser elegido
 *      - state: PROCESS_READY
 *      - next: nullptr (para wait queues de semáforos)
 *      - block_reason: BLOCK_REASON_NONE
 *   4. Configurar contexto de ejecución (ret_from_fork)
 *   
 *   El proceso creado estará listo para ser elegido por el
 *   Round-Robin scheduler con asignación de quantum.
 */
long create_process(void (*fn)(void*), void *arg, int priority, const char *name) {
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
    p->context.x20 = (unsigned long)arg;
    p->context.pc = (unsigned long)ret_from_fork;
    p->context.sp = (unsigned long)stack + 4096;

    num_process++;

    return pid;
}

/**
 * @brief Crea un Hilo del Kernel (Kernel Thread)
 * @param fn Función a ejecutar
 * @param priority Prioridad inicial del proceso
 * @param name Nombre descriptivo del proceso
 * @return PID del proceso creado, -1 en caso de error
 * 
 * @details
 *   En BareMetalM4 (v0.5), como no hay separación de memoria virtual por proceso,
 *   todos los procesos son técnicamente hilos del kernel que comparten espacio de direcciones.
 *   
 *   Esta función es un wrapper sobre create_process() que pasa nullptr como argumento.
 */
long create_thread(void (*fn)(void*), int priority, const char *name) {
    /* Wrapper sobre create_process para cumplir con la semántica del Tema 2 */
    return create_process(fn, nullptr, priority, name);
}

/**
 * @brief Inicializa el subsistema de gestión de procesos
 * 
 * @details
 *   Configura el proceso 0 (kernel/idle) y prepara las estructuras
 *   para la creación de nuevos procesos.
 *   
 *   El PID 0 es el proceso IDLE que:
 *   - Se ejecuta cuando no hay otros procesos READY
 *   - Tiene prioridad 0 (la más baja)
 *   - Ejecuta WFI (Wait For Interrupt) para ahorrar energía
 *   - No tiene quantum asignado (no necesita preemption)
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
 * 
 * @details
 *   Marca el proceso como ZOMBIE y llama a schedule().
 *   El proceso ZOMBIE será limpiado posteriormente por free_zombie()
 *   en el loop principal del kernel, que liberará su stack y
 *   marcará el PCB como UNUSED para reutilización.
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
 * 
 * @details
 *   Placeholder para lógica que debe ejecutarse inmediatamente después
 *   de un context switch (cambio de contexto).
 *   
 *   En implementaciones futuras podría incluir:
 *   - Actualización de estadísticas de scheduling
 *   - Notificaciones de cambio de proceso
 *   - Lógica de sincronización post-fork
 */
void schedule_tail(void) {
    /* Placeholder para lógica post-context-switch */
}

/**
 * @brief Libera recursos de procesos terminados (ZOMBIE reaper)
 * 
 * @details
 *   Función invocada periódicamente desde el loop principal del kernel.
 *   Recorre la tabla de procesos y para cada ZOMBIE:
 *   1. Libera su stack mediante kfree()
 *   2. Marca el PCB como UNUSED (reciclable)
 *   3. Decrementa num_process
 *   
 *   Esto completa el ciclo de vida: UNUSED → READY → RUNNING/BLOCKED → ZOMBIE → UNUSED
 */
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

/* ========================================================================== */
/* SOPORTE PARA MODO USUARIO (EL0)                                          */
/* ========================================================================== */

/**
 * @brief Estructura de contexto para transición a modo usuario
 * 
 * @details
 *   Almacena los registros necesarios para saltar de EL1 (Kernel) a EL0 (Usuario):
 *   - pc: Program Counter donde inicia el código de usuario
 *   - sp: Stack Pointer para el stack de usuario
 */
struct user_context {
    unsigned long pc;
    unsigned long sp;
};

/**
 * @brief Función wrapper que realiza la transición a modo usuario
 * @param arg Puntero a user_context con pc y sp del usuario
 * 
 * @details
 *   Esta función se ejecuta en modo kernel (EL1) y es el punto de entrada
 *   para procesos que necesitan ejecutarse en modo usuario (EL0).
 *   
 *   Flujo:
 *   1. Recibe el contexto de usuario (pc y sp)
 *   2. Llama a move_to_user_mode() (ensamblador) que:
 *      - Configura SPSR_EL1 para retornar a EL0
 *      - Carga pc y sp en los registros correspondientes
 *      - Ejecuta ERET para saltar a modo usuario
 */
void kernel_to_user_wrapper(void *arg) {
    struct user_context *ctx = (struct user_context *)arg;

    kprintf("[KERNEL] Saltando a Modo Usuario (EL0)...\n");
    move_to_user_mode(ctx->pc, ctx->sp);
}

/**
 * @brief Crea un proceso de usuario (EL0)
 * @param user_fn Función que se ejecutará en modo usuario
 * @param name Nombre descriptivo del proceso
 * @return PID del proceso creado, -1 en caso de error
 * 
 * @details
 *   Crea un proceso que ejecutará código en modo usuario (EL0) en lugar
 *   del modo kernel (EL1). Esto permite:
 *   - Protección de memoria (el usuario no puede acceder a zonas del kernel)
 *   - Aislamiento de fallos (un crash de usuario no afecta al kernel)
 *   - Menor privilegio (el usuario no puede ejecutar instrucciones sensibles)
 *   
 *   Flujo de creación:
 *   1. Asigna un stack de 4KB para el usuario
 *   2. Crea una estructura user_context con pc y sp
 *   3. Crea un proceso kernel que ejecuta kernel_to_user_wrapper()
 *   4. El wrapper realizará la transición a EL0 mediante move_to_user_mode()
 *   
 *   INTEGRACION CON DEMAND PAGING:
 *   Los procesos de usuario son ideales para demostrar demand paging, ya que
 *   pueden acceder a direcciones virtuales que provocan Page Faults manejados
 *   por handle_fault() en sys.c.
 */
long create_user_process(void (*user_fn)(void), const char *name) {
    /* 1. Stack de Usuario */
    void *user_stack = kmalloc(4096);

    /* 2. Contexto de arranque */
    struct user_context *ctx = (struct user_context *)kmalloc(sizeof(struct user_context));
    ctx->pc = (unsigned long)user_fn;
    ctx->sp = (unsigned long)user_stack + 4096;

    /* 3. Crear proceso Kernel que saltara a User */
    return create_process(kernel_to_user_wrapper, ctx, 10, name);
}