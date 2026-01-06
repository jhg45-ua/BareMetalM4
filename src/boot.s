/* src/boot.S - Punto de Entrada del Kernel Bare-Metal */

/*
 * FLUJO DE EJECUCCION:
 *   1. CPU enciende -> PC apunta a _start (definido en link.ld)
 *   2. Leemos MPIDR_EL1 para identificar el core
 *   3. Si NO somos core 0 -> loop infinito (WFE)
 *   4. Si SI somos core 0 -> configurar el stack y saltar a kernel()
 */

.section .text
.global _start

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

    /* PASO 3: Saltar a codigo C */
    bl kernel               /* Branch with Link a kernel() */

    /* PASO 4: Si kernel() retorna (ERROR) -> detener sistema */
hang:
    wfe                     /* Wait For Event (bajo consumo) */
    b hang                  /* Loop infinito */