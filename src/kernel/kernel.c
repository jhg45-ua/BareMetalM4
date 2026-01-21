/**
 * @file kernel.c
 * @brief Punto de entrada principal del kernel BareMetalM4
 * 
 * @details
 *   Inicializa el sistema operativo y arranca el scheduler.
 *   Este archivo coordina la inicialización de todos los subsistemas.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
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
 * @details
 *   Inicializa:
 *   1. Sistema de memoria (MMU + Heap)
 *   2. Sistema de procesos (PID 0)
 *   3. Timer e interrupciones (GIC)
 *   4. Shell interactivo
 *   5. Loop IDLE principal (WFI)
 */
void kernel(void) {
    kprintf("¡¡¡Hola desde BareMetalM4!!!\n");
    kprintf("Sistema Operativo iniciando...\n");
    kprintf("Planificador por Prioridades\n");

    /* 1. Inicializar Memoria (MMU y Heap) */
    init_memory_system();

    /* 2. Inicializar Gestión de Procesos (PID 0) */
    init_process_system();

    /* 3. Inicializar Timers e Interrupciones */
    timer_init();

    /* 4. Lanzar servicios del sistema (Shell) */
    if (create_process((void(*)(void*))shell_task, 0, 1, "Shell") < 0) {
        kprintf("FATAL: No se pudo iniciar el Shell.\n");
        while(1);
    }

    /* 5. Ceder control al Scheduler */
    kprintf("--- Inicializacion de Kernel Completada. Pasando control al Planificador ---\n");

    /* ========================================================================== */
    /* LOOP PRINCIPAL (IDLE)                                                     */
    /* ========================================================================== */

    while(1) {
        /* Liberar procesos zombie */
        free_zombie();

        /* Wait For Interrupt (bajo consumo) */
        asm volatile("wfi");
    }
}
