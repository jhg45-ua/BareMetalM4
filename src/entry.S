/* src/entry.S */
.global cpu_switch_to
.global irq_handler_stub
.global enable_interrupts

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

/* Habilitar interrupciones (DAIF) */
enable_interrupts:
    msr daifclr, #2
    ret

/* Wrapper de la Interrupción */
irq_handler_stub:
    /* Guardar TODO (Caller-saved + Callee-saved) porque interrumpimos en cualquier momento */
    sub sp, sp, #256
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    stp x4, x5, [sp, #16 * 2]
    stp x6, x7, [sp, #16 * 3]
    stp x8, x9, [sp, #16 * 4]
    stp x10, x11, [sp, #16 * 5]
    stp x12, x13, [sp, #16 * 6]
    stp x14, x15, [sp, #16 * 7]
    stp x16, x17, [sp, #16 * 8]
    stp x18, x19, [sp, #16 * 9]
    stp x20, x21, [sp, #16 * 10]
    stp x22, x23, [sp, #16 * 11]
    stp x24, x25, [sp, #16 * 12]
    stp x26, x27, [sp, #16 * 13]
    stp x28, x29, [sp, #16 * 14]
    
    /* Guardar estado de excepción */
    mrs x21, spsr_el1
    mrs x22, elr_el1
    stp x30, x21, [sp, #16 * 15] 
    str x22, [sp, #16 * 16]

    /* Llamar a C */
    bl handle_timer_irq

    /* Restaurar estado */
    ldr x22, [sp, #16 * 16]
    ldp x30, x21, [sp, #16 * 15]
    msr elr_el1, x22
    msr spsr_el1, x21

    ldp x28, x29, [sp, #16 * 14]
    ldp x26, x27, [sp, #16 * 13]
    ldp x24, x25, [sp, #16 * 12]
    ldp x22, x23, [sp, #16 * 11]
    ldp x20, x21, [sp, #16 * 10]
    ldp x18, x19, [sp, #16 * 9]
    ldp x16, x17, [sp, #16 * 8]
    ldp x14, x15, [sp, #16 * 7]
    ldp x12, x13, [sp, #16 * 6]
    ldp x10, x11, [sp, #16 * 5]
    ldp x8, x9, [sp, #16 * 4]
    ldp x6, x7, [sp, #16 * 3]
    ldp x4, x5, [sp, #16 * 2]
    ldp x0, x1, [sp, #16 * 0]
    add sp, sp, #256
    
    eret