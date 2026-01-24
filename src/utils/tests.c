/**
 * @file tests.c
 * @brief Implementación de funciones de prueba del sistema
 * 
 * @details
 *   Suite de pruebas para validar las funcionalidades implementadas:
 *   
 *   SCHEDULER:
 *   - Round-Robin con Quantum (expropriación por tiempo)
 *   - Prioridades con aging (evita starvation)
 *   - Sleep y wake-up eficientes
 *   
 *   SINCRONIZACIÓN:
 *   - Semáforos con Wait Queues (NO busy-wait)
 *   - Bloqueo eficiente sin consumo de CPU
 *   
 *   MEMORIA:
 *   - Paginación por demanda (Demand Paging)
 *   - Asignación automática de páginas en Page Fault
 *   - Heap dinámico con kmalloc/kfree
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
 */

#include "../../include/tests.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/process.h"
#include "../../include/mm/malloc.h"
#include "../../include/semaphore.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                         */
/* ========================================================================== */

/* Obtiene el valor del registro SCTLR_EL1 (System Control Register) */
extern unsigned long get_sctlr_el1(void);

/* Habilita las interrupciones IRQ en el procesador */
extern void enable_interrupts(void);

/**
 * @brief Ejecuta pruebas del sistema de memoria dinámica
 * 
 * @details
 *   Verifica el funcionamiento del gestor de heap:
 *   - Estado de la MMU consultando SCTLR_EL1 (bit M = MMU habilitada)
 *   - Asignación dinámica con kmalloc (algoritmo First-Fit)
 *   - Escritura y lectura en memoria heap
 *   - Liberación de memoria con kfree
 *   
 *   Nota: Para probar demand paging, usar test_demand()
 */
void test_memory() {
    unsigned long sctlr = get_sctlr_el1();
    kprintf("Estado actual de SCTLR_EL1: 0x%x\n", sctlr);

    kprintf("   [TEST] Ejecutando tests de memoria...\n");

    /* Asignamos un bloque de 16 bytes */
    char *ptr = (char *)kmalloc(16);
    if (ptr) {
        k_strncpy(ptr, "TestOK", 16);
        kprintf("   [TEST] Malloc 16b: %s (Dir: 0x%x)\n", ptr, ptr);
        kfree(ptr);
    } else {
        kprintf("   [TEST] FALLO: Malloc devolvió NULL\n");
    }
}

/* ========================================================================== */
/* PROCESOS DE USUARIO (PROCESOS DE PRUEBA)                                 */
/* ========================================================================== */

/**
 * @brief Primer proceso de usuario (multitarea cooperativa con sleep)
 * 
 * @details
 *   Proceso lento que cuenta hasta 10 con sleep de 70 ticks.
 *   Demuestra el scheduler Round-Robin con prioridades:
 *   - Cede la CPU voluntariamente con sleep()
 *   - Entra en wait queue durante el sleep (NO busy-wait)
 *   - Es despertado por timer_tick() cuando wake_up_time llega
 *   
 *   Incrementa el contador dos veces por iteración (bug intencionado).
 */
void proceso_1(void) {
    enable_interrupts();
    int c = 0;
    while(c < 10) {
        kprintf("[P1] Proceso Lento (Cuenta: %d)\n", c++);
        sleep(70);
        c++;
    }
}

/**
 * @brief Segundo proceso de usuario (multitarea cooperativa con sleep)
 * 
 * @details
 *   Proceso rápido que cuenta hasta 20 con sleep de 10 ticks.
 *   Demuestra el scheduler Round-Robin con prioridades:
 *   - Despierta mucho más frecuentemente que proceso_1
 *   - Deberías ver muchas más impresiones de P2 que de P1
 *   - Valida el manejo eficiente de múltiples procesos durmiendo
 *   
 *   Incrementa el contador dos veces por iteración (bug intencionado).
 */
void proceso_2(void) {
    enable_interrupts();
    int c = 0;
    while(c < 20) {
        kprintf("     [P2] Proceso Rapido (Cuenta: %d)\n", c++);
        sleep(10);
        c++;
    }
}

/**
 * @brief Tarea que cuenta hasta 3 y termina
 *
 * @details
 *   Proceso temporal que demuestra la terminación automática.
 *   Tras 3 iteraciones de sleep(15), la función retorna.
 *   El wrapper 'ret_from_fork' captura ese retorno y llama a exit().
 */
void proceso_mortal(void) {
    enable_interrupts();

    /* Duerme 3 veces y termina */
    for (int i = 0; i < 3; i++) {
        sleep(15);
    }

    /* Al terminar el for, la función hace 'return'.
       El wrapper 'ret_from_fork' captura ese retorno y llama a 'exit()'. */
}

/* ========================================================================== */
/* LANZADORES DE PRUEBAS                                                     */
/* ========================================================================== */

/**
 * @brief Lanza pruebas de creación y destrucción de procesos
 * 
 * @details
 *   Crea 3 procesos mortales que terminan automáticamente.
 *   Valida el ciclo de vida completo:
 *   - UNUSED -> READY -> RUNNING -> SLEEPING -> ZOMBIE -> UNUSED
 *   - Limpieza automática de zombies por free_zombie()
 *   - Liberación de memoria del stack (kmalloc/kfree)
 */
void test_processes(void) {
    kprintf("\n[TEST] --- Probando Ciclo de Vida (Zombies/Exit) ---\n");

    /* Lanzamos 3 procesos que morirán pronto */
    create_process((void(*)(void*))proceso_mortal, nullptr, 10, "Mortal_A");
    create_process((void(*)(void*))proceso_mortal, nullptr, 10, "Mortal_B");
    create_process((void(*)(void*))proceso_mortal, nullptr, 10, "Mortal_C");
}

/**
 * @brief Lanza pruebas de scheduler Round-Robin con sleep
 * 
 * @details
 *   Crea dos procesos con diferentes tiempos de sleep:
 *   - P1 (Lento): Duerme 70 ticks entre iteraciones
 *   - P2 (Rápido): Duerme 10 ticks entre iteraciones
 *   
 *   Valida:
 *   - Gestión eficiente de procesos SLEEPING (no consumen CPU)
 *   - Despertar automático en timer_tick() cuando se cumple wake_up_time
 *   - Algoritmo de prioridades con aging para evitar starvation
 */
void test_scheduler(void) {
    kprintf("\n[TEST] --- Probando Multitarea y Sleep ---\n");

    /* P1 duerme mucho, P2 duerme poco */
    create_process((void(*)(void*))proceso_1, nullptr, 20, "Lento");
    create_process((void(*)(void*))proceso_2, nullptr, 10, "Rapido");
}

/* ========================================================================== */
/* PRUEBAS DE SYSCALLS Y SEGURIDAD                                           */
/* ========================================================================== */

/**
 * @brief Proceso de usuario en EL0 que ejecuta syscalls
 * 
 * @details
 *   Demuestra el uso de llamadas al sistema desde nivel de usuario:
 *   - SYS_WRITE (0): Imprime mensaje en consola
 *   - SYS_EXIT (1): Termina el proceso limpiamente
 *   
 *   Utiliza ensamblador inline para invocar syscalls via SVC.
 */
void user_task() {
    char *msg = "\n[USER] Hola desde EL0! Soy un proceso restringido.\n";

    /* Syscall Write (0) */
    asm volatile(
        "mov x8, #0\n"      // Número de syscall en x8
        "mov x19, %0\n"     // Argumento (mensaje) en x19
        "svc #0\n"          // Supervisor Call
        : : "r"(msg) : "x8", "x19"
    );

    /* Bucle para probar multitarea */
    for(int i=0; i<10000000; i++) asm volatile("nop");

    /* Syscall Exit (1) */
    asm volatile(
        "mov x8, #1\n"      // Número de syscall en x8
        "mov x19, #0\n"     // Código de salida en x19
        "svc #0\n"          // Supervisor Call
        : : : "x8", "x19"
    );
}

/**
 * @brief Proceso que intenta violar segmentación de memoria
 * 
 * @details
 *   Prueba de robustez del manejo de excepciones.
 *   Intenta escribir en dirección NULL (0x0), lo cual debería:
 *   - Generar un Data Abort / Page Fault
 *   - Ser capturado por handle_fault()
 *   - Terminar el proceso sin colapsar el sistema
 *   
 *   Si se imprime el mensaje final, el manejo de excepciones falló.
 */
void kamikaze_test() {
    kprintf("\n[KAMIKAZE] Soy un proceso malo. Voy a escribir en NULL...\n");

    /* Intentamos escribir en la dirección 0x0 (prohibida/no mapeada) */
    int *p = (int *)nullptr;
    *p = 1234;  /* ¡CRASH esperado! */

    kprintf("[KAMIKAZE] Si lees esto, la seguridad ha fallado\n");

    /* Salida normal (no deberíamos llegar aquí) */
    asm volatile("mov x8, #1; mov x19, #0; svc #0");
}

/* ========================================================================== */
/* PRUEBAS DE ROUND-ROBIN CON QUANTUM (PREEMPTION)                         */
/* ========================================================================== */

/**
 * @brief Proceso egoísta que consume CPU indefinidamente
 * 
 * @details
 *   Entra en un bucle infinito sin ceder la CPU voluntariamente.
 *   Prueba el Round-Robin con Quantum implementado:
 *   
 *   FUNCIONAMIENTO:
 *   - Cada proceso recibe un quantum (ej. 10 ticks)
 *   - En cada timer_tick(), el quantum se decrementa
 *   - Cuando quantum llega a 0, se marca need_reschedule
 *   - El scheduler expropria el proceso y asigna la CPU a otro
 *   
 *   Sin quantum, este proceso monopolizaría el sistema y mataría el SO.
 *   Con quantum, el shell y otros procesos siguen funcionando.
 */
void tarea_egoista(void) {
    kprintf("   [EGO] Soy el PID %d y entro en bucle infinito SIN sleep...\n", current_process->pid);
    kprintf("   [EGO] Intenta usar el Shell mientras yo corro. Si puedes el quantum funciona!!!!\n");

    /* Bucle infinito sucio. Sin quantum, esto mata el SO */
    volatile unsigned long i = 0;
    while (1) {
        i++;
        /* No llamamos a sleep() ni a nada que ceda la CPU */
    }
}

/**
 * @brief Lanza prueba de Round-Robin con Quantum
 * 
 * @details
 *   Crea un proceso egoísta que intenta monopolizar la CPU.
 *   
 *   RESULTADO ESPERADO:
 *   - El proceso egoísta corre durante su quantum
 *   - Tras agotar su quantum, el scheduler lo expropria
 *   - El shell y otros procesos pueden ejecutarse normalmente
 *   - Demuestra que el Round-Robin con Quantum funciona correctamente
 *   
 *   Sin esta implementación, el sistema quedaría congelado.
 */
void test_quantum(void) {
    kprintf("\n[TEST] --- Probando Round-Robin (Preemption) ---\n");
    /* Pasamos NULL al argumento si ya arreglaste create_process */
    create_process((void(*)(void*))tarea_egoista, nullptr, 10, "Egoista");
}

/* ========================================================================== */
/* PRUEBAS DE SINCRONIZACIÓN - SEMÁFOROS CON WAIT QUEUES                   */
/* ========================================================================== */

/* Semáforo compartido para pruebas de sincronización */
struct semaphore sem_prueba;

/**
 * @brief Proceso que mantiene un semáforo ocupado
 * 
 * @details
 *   Adquiere el semáforo con sem_wait(), duerme 5 segundos (500 ticks)
 *   y lo libera con sem_signal().
 *   
 *   Mientras lo tiene tomado, otros procesos que intenten adquirirlo
 *   serán bloqueados (BLOCKED) y agregados a la wait queue del semáforo.
 */
void tarea_holder(void) {
    kprintf("   [HOLDER] Tomando semaforo y durmiendo 5 segundos...\n");
    sem_wait(&sem_prueba);

    sleep(500);

    kprintf("   [HOLDER] Liberando semaforo\n");
    sem_signal(&sem_prueba);
}


/**
 * @brief Proceso que espera por un semáforo ocupado
 * 
 * @details
 *   Intenta adquirir el semáforo ya tomado por holder.
 *   
 *   COMPORTAMIENTO CON WAIT QUEUES:
 *   - sem_wait() detecta que el semáforo está tomado (count = 0)
 *   - El proceso se marca como BLOCKED (block_reason = BLOCK_REASON_SEM)
 *   - Se añade a la wait queue del semáforo
 *   - NO consume CPU mientras espera (NO busy-wait)
 *   - Cuando holder llama a sem_signal(), este proceso es despertado
 *   - Cambia a READY y vuelve a competir por la CPU
 *   
 *   Esta implementación es EFICIENTE, a diferencia del busy-wait.
 */
void tarea_waiter(void) {
    kprintf("   [WAITER] Intentando tomar semaforo (deberia bloquearme)...\n");

    /* Aquí debería bloquearse y NO consumir CPU */
    sem_wait(&sem_prueba);

    kprintf("   [WAITER] ¡Conseguido! He despertado.\n");
    sem_signal(&sem_prueba);
}

/**
 * @brief Lanza prueba de semáforos con Wait Queues (NO busy-wait)
 * 
 * @details
 *   Crea dos procesos que compiten por el mismo semáforo:
 *   - Holder: Toma el semáforo y lo mantiene ocupado 5 segundos
 *   - Waiter: Intenta tomarlo y debe bloquearse eficientemente
 *   
 *   VALIDACIÓN:
 *   - Waiter NO consume CPU mientras espera (estado BLOCKED)
 *   - Waiter es despertado automáticamente cuando Holder libera
 *   - Sincronización correcta sin busy-wait
 *   - Eficiencia: Los procesos bloqueados no aparecen en schedule()
 *   
 *   Mejora significativa vs. implementaciones antiguas con busy-wait.
 */
void test_semaphores_efficiency(void) {
    kprintf("\n[TEST] --- Probando Wait Queues (Eficiencia) ---\n");

    /* Inicializamos semáforo a 1 */
    sem_init(&sem_prueba, 1);

    /* Lanzamos al que tiene el candado */
    create_process((void(*)(void*))tarea_holder, (void*)nullptr, 10, "Holder");

    /* Lanzamos al que espera. Dale menos prioridad o lánzalo un pelín después */
    /* Nota: Si lanzamos al waiter inmediatamente, aseguramos que se bloquee */
    create_process((void(*)(void*))tarea_waiter, (void*)nullptr, 10, "Waiter");
}

/* ========================================================================== */
/* PRUEBAS DE MEMORIA VIRTUAL - PAGINACIÓN POR DEMANDA                     */
/* ========================================================================== */

/**
 * @brief Prueba de paginación por demanda (Demand Paging)
 * 
 * @details
 *   Intenta escribir en memoria no mapeada (0x50000000).
 *   
 *   CON DEMAND PAGING IMPLEMENTADO:
 *   1. Se produce un Page Fault (Data Abort) controlado
 *   2. El handler de excepciones detecta que es un fallo de página
 *   3. El kernel asigna la página físicamente bajo demanda
 *   4. Actualiza las tablas de páginas (PTE)
 *   5. La instrucción se reintenta y la escritura se completa con éxito
 *   6. Se imprime: "Exito! El valor guardado es: 42"
 *   
 *   SIN DEMAND PAGING:
 *   - El proceso sería terminado por handle_fault()
 *   - El mensaje de éxito nunca aparecería
 *   
 *   Esta técnica permite asignar memoria solo cuando realmente se usa,
 *   optimizando el uso de RAM (lazy allocation).
 */
void test_demand(void) {
    kprintf("Escribiendo en memoria no mapeada...\n");
    unsigned long *peligro = (unsigned long *)0x50000000;
    *peligro = 42; /* ¡BUM! Esto lanzará un Page Fault */
    kprintf("Exito! El valor guardado es: %d\n", *peligro);
}

