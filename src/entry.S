/**
 * @file entry.S
 * @brief Context switch y manejo de interrupciones (ARM64)
 *
 * @details
 *   Rutinas críticas en ensamblador:
 *   - Cambio de contexto entre procesos (cpu_switch_to)
 *   - Habilitar IRQs (enable_interrupts)
 *   - Stub de interrupciones IRQ (irq_handler_stub)
 *   - Entrada de procesos recién creados (ret_from_fork)
 *
 *   Guarda y restaura contexto completo, incluyendo registros
 *   callee-saved y estado de excepción (ELR, SPSR) para permitir
 *   preemptive multitasking.
 *
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

.global cpu_switch_to
.global irq_handler_stub
.global enable_interrupts
.global ret_from_fork
.global exit

/**
 * cpu_switch_to - Cambio de contexto entre procesos
 *
 * @param x0 Puntero al PCB del proceso saliente (prev)
 * @param x1 Puntero al PCB del proceso entrante (next)
 *
 * Guarda registros callee-saved (x19-x30, SP) de 'prev' y restaura
 * los de 'next'.
 */
cpu_switch_to:
    /* Guardar contexto de 'prev' */
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]    /* FP y LR */
    mov x9, sp
    str x9, [x0, #96]          /* SP */

    /* Restaurar contexto de 'next' */
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x9, [x1, #96]
    mov sp, x9

    ret    /* Saltar a la direccion en x30 (LR de 'next') */

/**
 * enable_interrupts - Habilita interrupciones IRQ
 *
 * Limpia el bit I del registro DAIF para permitir IRQs.
 */
enable_interrupts:
    msr daifclr, #2    /* Limpiar bit IRQ (bit 1) */
    ret

/**
 * irq_handler_stub - Punto de entrada para interrupciones IRQ
 *
 * Guarda todo el contexto (x0-x30, ELR, SPSR), llama a handle_timer_irq()
 * en C y restaura el contexto (posiblemente de otro proceso si schedule()
 * realizó cambio de contexto).
 */
irq_handler_stub:
    /* Reservar espacio en stack (256 bytes) */
    sub sp, sp, #256
    
    /* Guardar todos los registros x0-x29 */
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
    
    /* Guardar registros de estado de excepcion */
    mrs x21, spsr_el1    /* Program Status Register */
    mrs x22, elr_el1     /* Exception Link Register (direccion de retorno) */
    stp x30, x21, [sp, #16 * 15]
    str x22, [sp, #16 * 16]

    /* Llamar al handler en C */
    bl handle_timer_irq

    /* Restaurar registros de estado */
    ldr x22, [sp, #16 * 16]
    ldp x30, x21, [sp, #16 * 15]
    msr elr_el1, x22
    msr spsr_el1, x21

    /* Restaurar todos los registros x0-x29 */
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
    ldp x2, x3, [sp, #16 * 1]
    ldp x0, x1, [sp, #16 * 0]
    
    /* Liberar espacio del stack */
    add sp, sp, #256
    
    /* Retornar de la excepcion */
    eret

/**
 * ret_from_fork - Punto de entrada para nuevos procesos
 *
 * @details
 *   Se ejecuta cuando un proceso recién creado obtiene CPU:
 *   1) Llama a schedule_tail() (hook post-fork)
 *   2) Recupera la función del usuario desde x19
 *   3) Salta a la función del proceso
 *   4) Si retorna, invoca exit() automáticamente
 */
ret_from_fork:
    bl schedule_tail

    mov x0, x19        /* Recuperamos la funcion real desde x19 */
    blr x0              /* Saltamos a la funcion del usuario: fn() */

    /* Si la funcion retorna (acaba), caemos aqui */
    bl exit             /* Llamamos a exit() para morir ordenadamente */

    /* Nunca deberiamos llegar aqui, pero por si acaso... */
    bl hang