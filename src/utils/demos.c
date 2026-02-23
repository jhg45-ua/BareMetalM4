/**
 * @file demos.c
 * @brief Codigo educativo de referencia - No invocado en la demo principal
 * 
 * @details
 *   Contiene funciones educativas que demuestran conceptos avanzados
 *   del sistema operativo pero que NO se invocan en la ejecucion normal:
 *   
 *   MODO USUARIO (EL0):
 *   - user_task(): Proceso que ejecuta syscalls desde EL0 (SVC)
 *   - kamikaze_test(): Intento de violacion de memoria (Data Abort)
 *   - create_user_process(): Creacion de procesos en modo usuario
 *   - kernel_to_user_wrapper(): Transicion EL1 -> EL0 via ERET
 *   
 *   Estas funciones se mantienen como referencia educativa para
 *   demostrar los conceptos del Tema 1 (Syscalls) y Tema 4 (Proteccion
 *   de memoria) del temario de Sistemas Operativos.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.6
 */

#include "../../include/utils/demos.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/mm/malloc.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                          */
/* ========================================================================== */

/* Transicion a modo usuario (entry.S) */
extern void move_to_user_mode(unsigned long pc, unsigned long sp);

/* ========================================================================== */
/* PROCESOS DE USUARIO EN EL0 (Referencia educativa)                         */
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
 *   
 *   NOTA: Codigo de referencia educativa. No invocado en la demo principal.
 */
void user_task(void) {
    char *msg = "\n[USER] Hola desde EL0! Soy un proceso restringido.\n";

    /* Syscall Write (0) */
    asm volatile(
        "mov x8, #0\n"      // Numero de syscall en x8
        "mov x19, %0\n"     // Argumento (mensaje) en x19
        "svc #0\n"          // Supervisor Call
        : : "r"(msg) : "x8", "x19"
    );

    /* Bucle para probar multitarea */
    for(int i=0; i<10000000; i++) asm volatile("nop");

    /* Syscall Exit (1) */
    asm volatile(
        "mov x8, #1\n"      // Numero de syscall en x8
        "mov x19, #0\n"     // Codigo de salida en x19
        "svc #0\n"          // Supervisor Call
        : : : "x8", "x19"
    );
}

/**
 * @brief Proceso que intenta violar segmentacion de memoria
 * 
 * @details
 *   Prueba de robustez del manejo de excepciones.
 *   Intenta escribir en direccion NULL (0x0), lo cual deberia:
 *   - Generar un Data Abort / Page Fault
 *   - Ser capturado por handle_fault()
 *   - Terminar el proceso sin colapsar el sistema
 *   
 *   NOTA: Codigo de referencia educativa. No invocado en la demo principal.
 */
void kamikaze_test(void) {
    kprintf("\n[KAMIKAZE] Soy un proceso malo. Voy a escribir en NULL...\n");

    /* Intentamos escribir en la direccion 0x0 (prohibida/no mapeada) */
    int *p = (int *)0;
    *p = 1234;  /* CRASH esperado! */

    kprintf("[KAMIKAZE] Si lees esto, la seguridad ha fallado\n");

    /* Salida normal (no deberiamos llegar aqui) */
    asm volatile("mov x8, #1; mov x19, #0; svc #0");
}

/* ========================================================================== */
/* SOPORTE PARA MODO USUARIO (EL0) - Referencia educativa                    */
/* ========================================================================== */

/**
 * @brief Estructura de contexto para transicion a modo usuario
 */
struct user_context {
    unsigned long pc;
    unsigned long sp;
};

/**
 * @brief Funcion wrapper que realiza la transicion a modo usuario
 * @param arg Puntero a user_context con pc y sp del usuario
 * 
 * @details
 *   Se ejecuta en modo kernel (EL1) y realiza la transicion a EL0
 *   mediante move_to_user_mode() que configura SPSR_EL1 y ejecuta ERET.
 *   
 *   NOTA: Codigo de referencia educativa. No invocado en la demo principal.
 */
void kernel_to_user_wrapper(void *arg) {
    struct user_context *ctx = (struct user_context *)arg;

    kprintf("[KERNEL] Saltando a Modo Usuario (EL0)...\n");
    move_to_user_mode(ctx->pc, ctx->sp);
}

/**
 * @brief Crea un proceso de usuario (EL0)
 * @param user_fn Funcion que se ejecutara en modo usuario
 * @param name Nombre descriptivo del proceso
 * @return PID del proceso creado, -1 en caso de error
 * 
 * @details
 *   Crea un proceso que ejecuta codigo en modo usuario (EL0).
 *   Demuestra proteccion de memoria y aislamiento de privilegios.
 *   
 *   NOTA: Codigo de referencia educativa. No invocado en la demo principal.
 */
long create_user_process(void (*user_fn)(void), const char *name) {
    /* 1. Stack de Usuario */
    void *user_stack = kmalloc(4096);

    /* 2. Contexto de arranque */
    struct user_context *ctx = (struct user_context *)kmalloc(sizeof(struct user_context));
    ctx->pc = (unsigned long)user_fn;
    ctx->sp = (unsigned long)user_stack + 4096;

    /* 3. Crear proceso Kernel que saltara a User */
    return create_process(kernel_to_user_wrapper, ctx, 10, name);
}
