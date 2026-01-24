/**
 * @file scheduler.c
 * @brief Planificador Round-Robin con Quantum y Prioridades
 * 
 * @details
 *   Implementa el scheduler híbrido del sistema operativo:
 *   
 *   ROUND-ROBIN CON QUANTUM (Preemptive Multitasking):
 *   - Cada proceso recibe un quantum de tiempo (DEFAULT_QUANTUM ticks)
 *   - El quantum se decrementa en cada timer_tick()
 *   - Cuando quantum = 0, se marca need_reschedule para expropiar
 *   - Previene monopolio de CPU por procesos egoístas
 *   
 *   PRIORIDADES CON AGING (Fairness):
 *   - Aging: Procesos READY envejecen (priority--)
 *   - Selección: Se elige el proceso con MENOR priority
 *   - Penalización: El proceso seleccionado aumenta su priority
 *   - Previene starvation de procesos de baja prioridad
 *   
 *   SLEEP Y WAKE-UP EFICIENTES:
 *   - Procesos durmiendo entran en estado BLOCKED
 *   - NO consumen CPU mientras duermen
 *   - timer_tick() despierta procesos cuando wake_up_time llega
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

/* Bandera global para indicar que se debe llamar a schedule()
   Marcada cuando un proceso agota su quantum o debe ceder la CPU */
volatile int need_reschedule = 0;

/**
 * @brief Verifica si hay un cambio de contexto pendiente
 * @return 1 si need_reschedule está activo, 0 en caso contrario
 */
int is_reschedule_pending () {
    return need_reschedule;
}

/* ========================================================================== */
/* SCHEDULER - ALGORITMO DE PRIORIDADES CON AGING                           */
/* ========================================================================== */

/**
 * @brief Planificador de procesos (Scheduler)
 * 
 * @details
 *   Implementa un algoritmo híbrido de scheduling:
 *   
 *   FASES DEL ALGORITMO:
 *   1. ENVEJECIMIENTO (Aging):
 *      - Todos los procesos READY (excepto el actual) envejecen
 *      - priority-- (menor valor = mayor prioridad)
 *      - Previene starvation
 *   
 *   2. SELECCIÓN:
 *      - Busca el proceso con MENOR priority entre READY y RUNNING
 *      - Ignora procesos BLOCKED (durmiendo o esperando semáforos)
 *      - Si no hay nadie, el kernel (PID 0) toma el control
 *   
 *   3. PENALIZACIÓN:
 *      - El proceso seleccionado aumenta su priority (+2)
 *      - Evita que monopolice la CPU en próximas rondas
 *      - Se le asigna un nuevo quantum completo
 *   
 *   4. CONTEXT SWITCH:
 *      - Si prev != next, se cambia el contexto con cpu_switch_to()
 *      - Guarda registros del proceso anterior
 *      - Restaura registros del proceso siguiente
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

    /* 3. Fase de Penalización y Asignación de Quantum */
    /* Penalizar tarea seleccionada para evitar monopolio */
    if (next->priority < 10) {
        next->priority += 2;
    }

    /* Asignar quantum completo (Round-Robin) */
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
/* TIMER TICK: ROUND-ROBIN, QUANTUM Y WAKE-UP                               */
/* ========================================================================== */

/* Contador global de ticks del sistema */
volatile unsigned long sys_timer_count = 0;

/**
 * @brief Manejador de ticks del timer
 * 
 * @details
 *   Función crítica llamada en cada interrupción del timer (IRQ).
 *   Implementa tres funcionalidades clave:
 *   
 *   1. ROUND-ROBIN CON QUANTUM:
 *      - Decrementa el quantum del proceso actual
 *      - Si quantum llega a 0, marca need_reschedule
 *      - El kernel_exit llamará a schedule() si need_reschedule=1
 *      - Implementa preemptive multitasking (expropriación)
 *   
 *   2. CONTABILIDAD DE CPU:
 *      - Incrementa cpu_time del proceso actual
 *      - Incrementa sys_timer_count (reloj global del sistema)
 *   
 *   3. WAKE-UP DE PROCESOS DURMIENDO:
 *      - Recorre todos los procesos BLOCKED
 *      - Si block_reason = SLEEP y wake_up_time <= now, los despierta
 *      - Los marca como READY para que schedule() pueda elegirlos
 */
void timer_tick(void) {
    sys_timer_count++;

    if (current_process->state == PROCESS_RUNNING) {
        current_process->cpu_time++;

        /* === ROUND-ROBIN CON QUANTUM === */
        /* Si no es el kernel (PID 0), aplicamos quantum */
        if (current_process->pid > 0) {
            current_process->quantum--;

            /* Se acabó el quantum del proceso actual */
            if (current_process->quantum <= 0) {
                /* Marcamos que se necesita cambio de contexto
                   NO llamamos a schedule() directamente desde aquí porque:
                   1. Estamos dentro de un IRQ handler
                   2. kernel_exit verificará need_reschedule y llamará a schedule()
                   3. Esto asegura que el cambio de contexto sea seguro */
                need_reschedule = 1;
                
                // kprintf("[SCHED] PID %d agotó su quantum. Marcando need_reschedule.\n", current_process->pid);
            }
        }
    }

    /* === WAKE-UP DE PROCESOS DURMIENDO === */
    /* Recorremos todos los procesos para despertar los que ya deben levantarse */
    for (int i = 0; i < MAX_PROCESS; i++) {
        if (process[i].state == PROCESS_BLOCKED) {
            /* Solo despertamos procesos que están durmiendo (sleep) */
            if (process[i].block_reason == BLOCK_REASON_SLEEP) {
                /* Verificamos si ya pasó su tiempo de despertar */
                if (process[i].wake_up_time <= sys_timer_count) {
                    process[i].state = PROCESS_READY;
                    process[i].block_reason = BLOCK_REASON_NONE;
                    // kprintf(" [KERNEL] Despertando proceso %d en tick %d\n", i, sys_timer_count);    // DEBUG
                }
            }
            /* Nota: Los procesos BLOCKED por semáforos (BLOCK_REASON_WAIT)
               se despiertan en sem_signal(), NO aquí */
        }
    }
}

/* ========================================================================== */
/* SLEEP: BLOQUEO EFICIENTE (NO BUSY-WAIT)                                 */
/* ========================================================================== */

/**
 * @brief Duerme el proceso actual durante un número de ticks del timer
 * @param ticks Número de ticks del timer a dormir
 * 
 * @details
 *   Implementa sleep eficiente sin busy-wait:
 *   
 *   1. Calcula wake_up_time = sys_timer_count + ticks
 *   2. Marca el proceso como BLOCKED con block_reason = SLEEP
 *   3. Llama a schedule() para ceder la CPU inmediatamente
 *   4. El proceso NO consume CPU mientras duerme
 *   5. timer_tick() lo despertará cuando wake_up_time llegue
 *   
 *   VENTAJA vs BUSY-WAIT:
 *   - En busy-wait: El proceso sigue consumiendo CPU en un bucle
 *   - Con BLOCKED: El scheduler lo ignora completamente
 *   - Mucho más eficiente en sistemas con múltiples procesos
 */
void sleep(unsigned int ticks) {
    current_process->wake_up_time = sys_timer_count + ticks;
    current_process->state = PROCESS_BLOCKED;
    current_process->block_reason = BLOCK_REASON_SLEEP;
    schedule();

    enable_interrupts();
}
