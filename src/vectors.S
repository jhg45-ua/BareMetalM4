/* src/vectors.S */
.global vectors
.global irq_handler_stub

.macro VENTRY label
    .align 7
    b \label
.endm

.align 11
vectors:
    /* Current EL with SP0 */
    VENTRY hang
    VENTRY hang
    VENTRY hang
    VENTRY hang

    /* Current EL with SPx (EL1h) - ¡AQUÍ TRABAJAMOS! */
    VENTRY hang
    VENTRY irq_handler_stub  /* Salto al timer */
    VENTRY hang
    VENTRY hang

    /* Lower EL (AArch64) */
    VENTRY hang
    VENTRY hang
    VENTRY hang
    VENTRY hang

    /* Lower EL (AArch32) */
    VENTRY hang
    VENTRY hang
    VENTRY hang
    VENTRY hang

hang:
    b hang