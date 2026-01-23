/**
 * @file tests.c
 * @brief Implementación de funciones de prueba
 * 
 * @details
 *   Pruebas del sistema para validar memoria, procesos
 *   y funcionalidades básicas del kernel.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#include "../../include/tests.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/process.h"
#include "../../include/mm/malloc.h"
#include "../../include/semaphore.h"

extern unsigned long get_sctlr_el1(void);
extern void enable_interrupts(void);

/**
 * @brief Ejecuta pruebas del sistema de memoria
 * 
 * @details
 *   Verifica:
 *   - Funcionamiento de kmalloc/kfree
 *   - Estado de la MMU (SCTLR_EL1)
 *   - Escritura y lectura en memoria dinámica
 */
void test_memory() {
    unsigned long sctlr = get_sctlr_el1();
    kprintf("Estado actual de SCTLR_EL1: 0x%x\n", sctlr);

    kprintf("   [TEST] Ejecutando tests de memoria...\n");

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
 * @brief Primer proceso de usuario (multitarea expropiativa)
 * 
 * @details
 *   Proceso lento que cuenta hasta 10 con sleep de 70 ticks.
 *   Demuestra el scheduler con procesos de diferentes velocidades.
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
 * @brief Segundo proceso de usuario (multitarea expropiativa)
 * 
 * @details
 *   Proceso rápido que cuenta hasta 20 con sleep de 10 ticks.
 *   Demuestra el scheduler con procesos de diferentes velocidades.
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
 * @brief Tarea que cuenta hasta 3 y muere
 *
 * @details
 *   Este proceso demuestra la terminación de procesos.
 *   Al finalizar, ret_from_fork captura el retorno y llama a exit().
 */
void proceso_mortal(void) {
    enable_interrupts();

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
 *   Crea 3 procesos mortales que terminan automáticamente
 *   para probar el ciclo de vida (zombie/exit).
 */
void test_processes(void) {
    kprintf("\n[TEST] --- Probando Ciclo de Vida (Zombies/Exit) ---\n");

    /* Lanzamos 3 procesos que morirán pronto */
    create_process((void(*)(void*))proceso_mortal, 0, 10, "Mortal_A");
    create_process((void(*)(void*))proceso_mortal, 0, 10, "Mortal_B");
    create_process((void(*)(void*))proceso_mortal, 0, 10, "Mortal_C");
}

/**
 * @brief Lanza pruebas de scheduler y sleep
 * 
 * @details
 *   Crea dos procesos con diferentes tiempos de sleep.
 *   P1 duerme mucho, P2 duerme poco. Deberías ver muchos P2 por cada P1.
 */
void test_scheduler(void) {
    kprintf("\n[TEST] --- Probando Multitarea y Sleep ---\n");

    /* P1 duerme mucho, P2 duerme poco */
    create_process((void(*)(void*))proceso_1, 0, 20, "Lento");
    create_process((void(*)(void*))proceso_2, 0, 10, "Rapido");
}

/* ========================================================================== */
/* PRUEBAS DE SYSCALLS Y SEGURIDAD                                           */
/* ========================================================================== */

/**
 * @brief Proceso de usuario en EL0 que ejecuta syscalls
 * 
 * @details
 *   Demuestra el uso de syscalls desde un proceso de usuario:
 *   - SYS_WRITE para imprimir mensaje
 *   - SYS_EXIT para terminar
 */
void user_task() {
    char *msg = "\n[USER] Hola desde EL0! Soy un proceso restringido.\n";

    /* Syscall Write (0) */
    asm volatile(
        "mov x8, #0\n"
        "mov x19, %0\n"
        "svc #0\n"
        : : "r"(msg) : "x8", "x19"
    );

    /* Bucle para probar multitarea */
    for(int i=0; i<10000000; i++) asm volatile("nop");

    /* Syscall Exit (1) */
    asm volatile(
        "mov x8, #1\n"
        "mov x19, #0\n"
        "svc #0\n"
        : : : "x8", "x19"
    );
}

/**
 * @brief Proceso que intenta violar segmentación de memoria
 * 
 * @details
 *   Prueba el manejo de excepciones intentando escribir en NULL.
 *   Debería ser terminado por handle_fault() sin colapsar el sistema.
 */
void kamikaze_test() {
    kprintf("\n[KAMIKAZE] Soy un proceso malo. Voy a escribir en NULL...\n");

    /* Intentamos escribir en la dirección 0x0 (prohibida/no mapeada) */
    int *p = (int *)0x0;
    *p = 1234;

    kprintf("[KAMIKAZE] Si lees esto, la seguridad ha fallado\n");

    /* Salida normal (no deberíamos llegar aquí) */
    asm volatile("mov x8, #1; mov x19, #0; svc #0");
}

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

void test_quantum(void) {
    kprintf("\n[TEST] --- Probando Round-Robin (Preemption) ---\n");
    /* Pasamos NULL al argumento si ya arreglaste create_process */
    create_process((void(*)(void*))tarea_egoista, (void*)0, 10, "Egoista");
}

struct semaphore sem_prueba;

void tarea_holder(void) {
    kprintf("   [HOLDER] Tomando semaforo y durmiendo 5 segundos...\n");
    sem_wait(&sem_prueba);

    sleep(500);

    kprintf("   [HOLDER] Liberando semaforo\n");
    sem_signal(&sem_prueba);
}


void tarea_waiter(void) {
    kprintf("   [WAITER] Intentando tomar semaforo (deberia bloquearme)...\n");

    /* Aquí debería bloquearse y NO consumir CPU */
    sem_wait(&sem_prueba);

    kprintf("   [WAITER] ¡Conseguido! He despertado.\n");
    sem_signal(&sem_prueba);
}

void test_semaphores_efficiency(void) {
    kprintf("\n[TEST] --- Probando Wait Queues (Eficiencia) ---\n");

    /* Inicializamos semáforo a 1 */
    sem_init(&sem_prueba, 1);

    /* Lanzamos al que tiene el candado */
    create_process((void(*)(void*))tarea_holder, (void*)0, 10, "Holder");

    /* Lanzamos al que espera. Dale menos prioridad o lánzalo un pelín después */
    /* Nota: Si lanzamos al waiter inmediatamente, aseguramos que se bloquee */
    create_process((void(*)(void*))tarea_waiter, (void*)0, 10, "Waiter");
}

void test_demand(void) {
    /* Test Page Fault */
    kprintf("Escribiendo en memoria no mapeada...\n");
    unsigned long *peligro = (unsigned long *)0x50000000;
    *peligro = 42; /* ¡BUM! Esto lanzará un Page Fault */
    kprintf("Exito! El valor guardado es: %d\n", *peligro);
}

