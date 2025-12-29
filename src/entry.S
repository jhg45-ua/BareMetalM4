/* src/entry.S */
.global cpu_switch_to

/* * void cpu_switch_to(struct pcb *prev, struct pcb *next);
    x0 = puntero al proceso que dejamos (prev)
    x1 = puntero al proceso al que vamos (next)
*/
cpu_switch_to:
    /* 1. GUARDAR el contexto de 'prev' (el proceso actual) */
    /* Guardamos x19-x30 y sp en la estructura apuntada por x0 */
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80] /* x29=FP, x30=LR (direccion de retorno) */

    /* Tanto stp como ldp trabajan con pares de registros y por tanto van con un offset múltiplo de 16 bytes */

    mov x9, sp
    str x9, [x0, #96] /* Guardar el Stack Pointer */

    /* 2. CARGAR el contexto de 'next' (el nuevo proceso) */
    /* Leemos desde la estructura apuntada por x1 */
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80] /* Esto restaura LR, así que al hacer ret saltaremos al código de la nueva tarea */

    ldr x9, [x1, #96]
    mov sp, x9 /* Restaurar el Stack Pointer */

    ret /* Saltar al nuevo proceso */
