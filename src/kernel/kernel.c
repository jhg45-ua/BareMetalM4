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
    if (create_process((void(*)(void*))shell_task, nullptr, 1, "Shell") < 0) {
        kprintf("FATAL: No se pudo iniciar el Shell.\n");
        while(1);
    }

    /* 6. Ceder control al Scheduler */
    kprintf("--- Inicializacion de Kernel Completada. Pasando control al Planificador ---\n");

    /* --- PRUEBA DE SYSCALL (SVC) --- */
    kprintf("[KERNEL] Probando SVC (System Call)...\n");

    /* Ejecutamos instrucción SVC #0 manualmente
       x8 = 0 (SYS_WRITE)
       x19 = dirección del string (Truco: en sys.c usaste x19 para el buffer)
       NOTA: Normalmente se usa x0, pero en tu sys.c leíste x19.
       Vamos a ajustarnos a tu sys.c por ahora.
    */
    char *msg = "Hola desde Syscall!\n";
    asm volatile(
        "mov x8, #0\n"        // Syscall 0 (WRITE)
        "mov x19, %0\n"       // Poner mensaje en x19 (según tu sys.c)
        "svc #0\n"            // Disparar excepción Síncrona
        : : "r"(msg) : "x8", "x19"
    );
    kprintf("[KERNEL] Regreso de SVC exitoso.\n");
    /* ------------------------------ */

    while(1) {
        free_zombie();

        asm volatile("wfi"); /* Wait For Interrupt (Loop del IDLE) */
    }
}
