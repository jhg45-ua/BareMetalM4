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
.global el1_sync
.global el0_sync
.global el0_irq
.global error_invalid


/* MACRO: Guardar todo el contexto (Registros x0-x30, ELR, SPSR) */
.macro kernel_entry
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

    mrs x21, spsr_el1
    mrs x22, elr_el1
    stp x30, x21, [sp, #16 * 15]
    str x22, [sp, #16 * 16]
.endm

/* MACRO: Restaurar contexto y volver (ERET) */
.macro kernel_exit
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
    ldp x2, x3, [sp, #16 * 1]
    ldp x0, x1, [sp, #16 * 0]

    add sp, sp, #256
    eret
.endm

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
    kernel_entry            // Macro: Guarda todo
    bl handle_timer_irq     // Llama a C
    kernel_exit             // Macro: Restaura y hace ERET

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

    mov x0, x20

    blr x19

    /* Si la funcion retorna (acaba), caemos aqui */
    bl exit             /* Llamamos a exit() para morir ordenadamente */

    /* Nunca deberiamos llegar aqui, pero por si acaso... */
    bl hang

/**
 * Syscall desde kernel
 */
 el1_sync:
    kernel_entry
    mrs x25, esr_el1
    lsr x24, x25, #26
    cmp x24, #0x15  // Es SVC??
    b.eq handle_svc
    b error_invalid

 /**
  * Syscall desde usuario
  */
  el0_sync:
    kernel_entry
    mrs x25, esr_el1
    lsr x24, x25, #26
    cmp x24, #0x15
    b.eq handle_svc
    b error_invalid

handle_svc:
    mov x0, sp  // arg 1: struct pt_regs *
    mov x1, x8  // arg 2: syscall
    bl syscall_handler
    kernel_exit

/* IRQ desde Usuario (reusamos el stub) */
el0_irq:
    b irq_handler_stub

error_invalid:
    b .

/* ========================================================== */
/* TRANSICIÓN A MODO USUARIO                                  */
/* ========================================================== */

.global move_to_user_mode

move_to_user_mode:
    // 1. Configurar PSTATE para EL0
    // m[3:0] = 0000 (EL0t)
    // DAIF = 0000 (Interrupciones habilitadas)
    mov x2, #0
    msr spsr_el1, x2

    // 2. Configurar donde aterrizar (PC)
    msr elr_el1, x0

    // 3. Configurar el stack del usuario
    msr sp_el0, x1

    // 4. Salto de fe
    eret