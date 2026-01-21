/**
 * @file boot.S
 * @brief Punto de entrada del kernel Bare-Metal ARM64
 * 
 * @details
 *   FLUJO DE EJECUCCION:
 *   1. CPU enciende -> PC apunta a _start (definido en link.ld)
 *   2. Leemos MPIDR_EL1 para identificar el core
 *   3. Si NO somos core 0 -> loop infinito (WFE)
 *   4. Si SI somos core 0 -> configurar el stack y saltar a kernel()
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

.section .text
.global _start
.global hang

/* 
 * _start - Punto de entrada del kernel
 * 
 * Esta es la primera instrucción que ejecuta el CPU al arrancar.
 * Filtra núcleos secundarios en sistemas SMP (solo core 0 continúa).
 * 
 * Secuencia:
 *   1. Identificar el core ID mediante MPIDR_EL1
 *   2. Core 0 configura stack y salta a kernel()
 *   3. Cores secundarios entran en bucle WFE (wait-for-event)
 */
_start:
    /* PASO 1: Identificar el Core (solo core 0 ejecuta el kernel) */
    mrs x0, MPIDR_EL1       /* Leer ID del procesador */
    and x0, x0, #3          /* Extraer Core ID (bits [1:0]) */
    cbz x0, master          /* Si core 0 -> saltar a master */
    b hang                  /* Cores secundarios -> dormir */

master:
    /* PASO 2: Configurar el Stack Pointer */
    ldr x0, =_stack_top     /* Cargar direccion del tope del stack */
    mov sp, x0              /* SP = Stack Pointer */

    ldr x0, =__bss_start
    ldr x1, =__bss_end
    sub x2, x1, x0          /* x2 = Tamaño de BSS */
    cbz x2, run_kernel      /* Si el tamaño es 0, saltar */

clear_bss_loop:
    str xzr, [x0], #8       /* Escribir 0 (xzr) y avanzar 8 bytes */
    sub x2, x2, #8           /* Restar 8 al contador */
    cbnz x2, clear_bss_loop /* Si no es 0, repetir */

run_kernel:
    /* PASO 3: Saltar a codigo C */
    bl kernel               /* Branch with Link a kernel() */

    /* PASO 4: Si kernel() retorna (ERROR) -> detener sistema */

/* 
 * hang - Bucle infinito para núcleos secundarios o errores fatales
 * 
 * Usa WFE (Wait For Event) para bajo consumo de energía.
 * Los cores secundarios permanecen aquí indefinidamente.
 */
hang:
    wfe                     /* Wait For Event (bajo consumo) */
    b hang                  /* Loop infinito */