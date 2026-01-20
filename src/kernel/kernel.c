/**
 * @file kernel.c
 * @brief Punto de entrada principal del kernel BareMetalM4
 * 
 * @details
 *   Inicializa el sistema operativo y arranca el scheduler.
 *   Este archivo coordina la inicialización de todos los subsistemas.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

#include "../../include/drivers/io.h"
#include "../../include/drivers/timer.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/shell.h"
#include "../../include/mm/mm.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS                                                        */
/* ========================================================================== */

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

    /* 2. Inicializar Memoria (MMU y Heap) */
    init_memory_system();

    /* 3. Inicializar Gestión de Procesos (PID 0) */
    init_process_system();

    /* 4. Inicializar Timers e Interrupciones */
    timer_init();

    /* 5. Lanzar servicios del sistema (Shell) */
    if (create_process(shell_task, 1, "Shell") < 0) {
        kprintf("FATAL: No se pudo iniciar el Shell.\n");
        while(1);
    }

    /* 6. Ceder control al Scheduler */
    kprintf("--- Inicializacion de Kernel Completada. Pasando control al Planificador ---\n");

    while(1) {
        free_zombie();

        asm volatile("wfi"); /* Wait For Interrupt (Loop del IDLE) */
    }
}
