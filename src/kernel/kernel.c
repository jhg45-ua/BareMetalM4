/**
 * @file kernel_main.c
 * @brief Punto de entrada principal del kernel BareMetalM4
 * 
 * @details
 *   Inicializa el sistema operativo y arranca el scheduler.
 *   Este archivo coordina la inicialización de todos los subsistemas.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

#include "../../include/types.h"
#include "../../include/sched.h"
#include "../../include/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/shell.h"
#include "../../include/kernel/kutils.h"
#include "../../include/mm.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS                                                        */
/* ========================================================================== */

/* Inicializa el timer del sistema (src/timer.c) */
extern void timer_init(void);

extern unsigned long get_sctlr_el1(void);

/* ========================================================================== */
/* PUNTO DE ENTRADA DEL KERNEL                                              */
/* ========================================================================== */

/**
 * @brief Punto de entrada principal del kernel
 * 
 * Inicializa:
 * 1. Proceso kernel (PID 0)
 * 2. Shell interactivo
 * 3. Procesos de prueba
 * 4. Timer del sistema
 * 5. Scheduler
 */
void kernel(void) {
    kprintf("¡¡¡Hola desde BareMetalM4!!!\n");
    kprintf("Sistema Operativo iniciando...\n");
    kprintf("Planificador por Prioridades\n");

    unsigned long sctlr = get_sctlr_el1();
    kprintf("Estado actual de SCTLR_EL1: 0x%x\n", sctlr);
    mem_init();

    /* Inicializamos Kernel como Proceso 0 */
    current_process = &process[0];
    current_process->pid = 0;
    current_process->state = PROCESS_RUNNING;
    current_process->priority = 20;
    k_strncpy(current_process->name, "Kernel", 16);
    num_process = 1;

    /* Crear shell interactivo */
    create_thread(shell_task, 1, "Shell");

    /* Crear procesos de prueba */
    // create_thread(proceso_1, 5, "Proceso 1");
    // create_thread(proceso_mortal, 5, "Proceso Mortal");

    /* Inicializar timer del sistema */
    timer_init();

    kprintf("Lanzando Scheduler...\n");
    
    /* Loop principal del kernel - espera interrupciones */
    while(1) {
        asm volatile("wfi");
    }
}
