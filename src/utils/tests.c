/**
 * @file tests.c
 * @brief Implementación de funciones de prueba
 * 
 * @details
 *   Pruebas del sistema para validar memoria, procesos
 *   y funcionalidades básicas del kernel.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

#include "../../include/tests.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/process.h"
#include "../../include/mm/malloc.h"

extern unsigned long get_sctlr_el1(void);

/* Habilita las interrupciones IRQ en el procesador */
extern void enable_interrupts(void);

/**
 * @brief Ejecuta pruebas del sistema de memoria
 * 
 * Verifica:
 * - Funcionamiento de kmalloc/kfree
 * - Estado de la MMU (SCTLR_EL1)
 * - Escritura y lectura en memoria dinámica
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
 */
void proceso_1(void) {
    enable_interrupts();
    int c = 0;
    while(1) {
        kprintf("[P1] Proceso 1 (Cuenta: %d)\n", c++);
        sleep(70);
    }
}

/**
 * @brief Segundo proceso de usuario (multitarea expropiativa)
 */
void proceso_2(void) {
    enable_interrupts();
    int c = 0;
    while(1) {
        kprintf("     [P2] Proceso 2 (Cuenta: %d)\n", c++);
        sleep(10);
    }
}

/**
 * @brief Tarea que cuenta hasta 3 y muere
 *
 * Este proceso demuestra la terminación de procesos.
 * Al finalizar, ret_from_fork captura el retorno y llama a exit().
 */
void proceso_mortal(void) {
    enable_interrupts();

    for (int i = 0; i < 3; i++) {
        // kprintf("     [MORTAL] Vida restante: %d\n", 3 - i);
        sleep(15); /* Duerme un poco */
    }

    // kprintf("     [MORTAL] Adios mundo cruel...\n");
    /* AQUÍ OCURRE LA MAGIA:
       Al terminar el for, la función hace 'return'.
       El wrapper 'ret_from_fork' captura ese retorno y llama a 'exit()'. */
}

/* ========================================================================== */
/* LANZADORES (Lo que el Kernel llama)                                        */
/* ========================================================================== */

/**
 * @brief Lanza pruebas de creación y destrucción
 */
void test_processes(void) {
    kprintf("\n[TEST] --- Probando Ciclo de Vida (Zombies/Exit) ---\n");

    /* Lanzamos 3 procesos que morirán pronto */
    create_process(proceso_mortal, 10, "Mortal_A");
    create_process(proceso_mortal, 10, "Mortal_B");
    create_process(proceso_mortal, 10, "Mortal_C");
}

/**
 * @brief Lanza pruebas de scheduler y sleep
 */
void test_scheduler(void) {
    kprintf("\n[TEST] --- Probando Multitarea y Sleep ---\n");

    /* P1 duerme mucho, P2 duerme poco. Deberías ver muchos P2 por cada P1 */
    create_process(proceso_1, 20, "Lento");
    create_process(proceso_2, 10, "Rapido");
}
