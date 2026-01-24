/**
 * @file kernel.c
 * @brief Punto de entrada principal del kernel BareMetalM4
 * 
 * @details
 *   Inicializa el sistema operativo y arranca el scheduler.
 *   Este archivo coordina la inicialización de todos los subsistemas:
 *   - Sistema de memoria (MMU + PMM + VMM para Demand Paging)
 *   - Sistema de procesos (con quantum para Round-Robin)
 *   - Interrupciones de timer (para preemption)
 *   - Shell del sistema
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
 */

#include "../../include/drivers/io.h"
#include "../../include/drivers/timer.h"
#include "../../include/kernel/process.h"
#include "../../include/shell/shell.h"
#include "../../include/mm/mm.h"
#include "../../include/fs/vfs.h"

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
 *   Secuencia de inicialización:
 *   1. Sistema de memoria (MMU + Heap + PMM + VMM)
 *      - Activa paginación y demand paging via Page Faults
 *   2. Sistema de procesos (PID 0 con quantum)
 *      - Inicializa estructuras PCB con soporte para Round-Robin
 *   3. Timer e interrupciones (GIC)
 *      - Configura IRQ periódicas que decrementan quantum
 *   4. Shell interactivo
 *      - Crea proceso de usuario con prioridad 1
 *   5. Loop IDLE principal (WFI)
 *      - Proceso 0 espera interrupciones en bajo consumo
 */
void kernel(void) {
    kprintf("¡¡¡Hola desde BareMetalM4!!!\n");
    kprintf("Sistema Operativo iniciando...\n");
    kprintf("Planificador por Prioridades\n");

    /* 1. Inicializar Memoria (MMU y Heap) */
    init_memory_system();

    /* Inicializar el RamDisk (Robamos 1MB de memoria virtual segura) */
    /* Usaremos una dirección arbitraria pero mapeada: 0x41000000 */
    ramfs_init(0x41000000, 1 * 1024 * 1024); /* 1MB de Disco Virtual */

    /* Crear archivos de prueba */
    vfs_create("readme.txt");
    vfs_create("config.sys");

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
