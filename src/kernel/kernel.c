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

#include "../../include/sched.h"
#include "../../include/drivers/io.h"
#include "../../include/drivers/timer.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/shell.h"
#include "../../include/kernel/kutils.h"
#include "../../include/mm/mm.h"
#include "../../include/mm/malloc.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS                                                        */
/* ========================================================================== */

extern unsigned long get_sctlr_el1(void);

extern char _end;

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

    /* --- DIAGNÓSTICO --- */
    kprintf("   [KERNEL] Inicio (.text): 0x40000000\n");
    
    /* Usamos '&' porque queremos la DIRECCIÓN del símbolo, no su valor */
    unsigned long heap_start = (unsigned long)&_end;
    
    kprintf("   [KERNEL] Fin del Kernel (_end): 0x%x\n", heap_start);
    kprintf("   [INFO] Listo para implementar kmalloc() a partir de aqui.\n");

    /* Inicializamos Kernel como Proceso 0 */
    current_process = &process[0];
    current_process->pid = 0;
    current_process->state = PROCESS_RUNNING;
    current_process->priority = 20;
    current_process->stack_addr = 0;
    k_strncpy(current_process->name, "Kernel", 16);
    num_process = 1;

    /* 1. Calcular memoria disponible */
    /* Vamos a darle 64 MB de Heap por ahora */
    unsigned long heap_size = 64 * 1024 * 1024;

    /* 2. Iniciar el Heap Manager */
    kheap_init(heap_start, heap_start + heap_size);

    /* 3. PRUEBA DE FUEGO: Pedir memoria dinámica */
    kprintf("   [TEST] Probando kmalloc()...\n");

    char *puntero1 = (char *)kmalloc(16);
    k_strncpy(puntero1, "Hola Dinamico!", 16);

    kprintf("   [TEST] ptr1 (16b): Dir 0x%x -> Contenido: '%s'\n", puntero1, puntero1);

    int *array_numeros = (int *)kmalloc(4 * sizeof(int)); // Array de 4 enteros
    array_numeros[0] = 1234;
    array_numeros[3] = 9999;

    kprintf("   [TEST] ptr2 (int*): Dir 0x%x -> Valor[0]: %d\n", array_numeros, array_numeros[0]);

    kprintf("   [TEST] Liberando memoria...\n");
    kfree(puntero1);
    kfree(array_numeros);
    /* ------------------------------------------- */

    /* Crear shell interactivo */
    create_process(shell_task, 1, "Shell");

    /* Inicializar timer del sistema */
    timer_init();

    kprintf("Lanzando Scheduler...\n");
    
    /* Loop principal del kernel - espera interrupciones */
    while(1) {
        asm volatile("wfi");
    }
}
