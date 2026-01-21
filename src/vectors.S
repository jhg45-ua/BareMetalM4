/**
 * @file vectors.S
 * @brief Tabla de vectores de excepciones ARM64
 * 
 * @details
 *   Define la tabla de vectores de excepciones (Exception Vector Table).
 *   Es una estructura de bajo nivel que el procesador ARM64 usa para saber
 *   a qué dirección saltar cuando ocurre una excepción (IRQ, SVC, etc).
 * 
 * @section EXCEPTION_MECHANISM
 *   CONCEPTO CLAVE:
 *   Cuando ocurre una interrupción o excepción:
 *   1. El CPU salta a: VBAR_EL1 + offset
 *   2. VBAR_EL1 es establecido por set_vbar_el1() en utils.S
 *   3. El offset depende del tipo de excepción y del nivel de privilegio
 * 
 * TABLA DE VECTORES ARM64 (16 entradas, 128 bytes cada una):
 * 
 *   ┌─────────────────────────────────────────────────────┐
 *   │  CURRENT EXCEPTION LEVEL WITH SP0 (EL1 usando SP0)  │
 *   │  (No se usa en este kernel, pero debe estar)        │
 *   ├─────────────────────────────────────────────────────┤
 *   │ +0x000: Synchronous    ↓                            │
 *   │ +0x080: IRQ            │                            │
 *   │ +0x100: FIQ            │ 128 bytes cada entrada     │
 *   │ +0x180: SError         ↓                            │
 *   ├─────────────────────────────────────────────────────┤
 *   │  CURRENT EXCEPTION LEVEL WITH SPx (EL1h) ← ¡AQUI!   │
 *   │  (Nuestro nivel: kernel en EL1 con su propio SP)    │
 *   ├─────────────────────────────────────────────────────┤
 *   │ +0x200: Synchronous                                 │
 *   │ +0x280: IRQ            ← Timer interrupt saltara aqui
 *   │ +0x300: FIQ                                         │
 *   │ +0x380: SError                                      │
 *   ├─────────────────────────────────────────────────────┤
 *   │  LOWER EXCEPTION LEVEL (AArch64)                    │
 *   │  (Procesos de usuario en EL0, 64-bit)               │
 *   ├─────────────────────────────────────────────────────┤
 *   │ +0x400-0x480: (4 entradas)                          │
 *   ├─────────────────────────────────────────────────────┤
 *   │  LOWER EXCEPTION LEVEL (AArch32)                    │
 *   │  (Procesos de usuario en EL0, 32-bit ARM)           │
 *   ├─────────────────────────────────────────────────────┤
 *   │ +0x600-0x680: (4 entradas)                          │
 *   └─────────────────────────────────────────────────────┘
 * 
 * 
 * @section EXCEPTION_TYPES
 *   TIPOS DE EXCEPCIONES:
 *   - Synchronous: Excepciones síncronas (divisor por 0, acceso memoria inválido)
 *   - IRQ: Interrupt Request (timer, I/O devices)
 *   - FIQ: Fast Interrupt Request (no usamos en este kernel)
 *   - SError: System Error (errores críticos de hardware)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

.global vectors
.global irq_handler_stub

/* Macro que crea una entrada de vector de excepcion alineada a 128 bytes */
.macro VENTRY label
    .align 7
    b \label
.endm

/* Tabla de vectores alineada a 2048 bytes (requisito ARM64) */
.align 11
vectors:
    /* GRUPO 1: Current EL with SP0 (EL1 usando SP0) */
    VENTRY hang    /* +0x000: Synchronous */
    VENTRY hang    /* +0x080: IRQ */
    VENTRY hang    /* +0x100: FIQ */
    VENTRY hang    /* +0x180: SError */

    /* GRUPO 2: Current EL with SPx (EL1h) - KERNEL MODE */
    VENTRY el1_sync          /* +0x200: Synchronous */
    VENTRY irq_handler_stub  /* +0x280: IRQ <- Timer interrupt */
    VENTRY hang              /* +0x300: FIQ */
    VENTRY hang              /* +0x380: SError */

    /* GRUPO 3: Lower EL (AArch64) - USER MODE 64-bit */
    VENTRY el0_sync /* +0x400: Synchronous */
    VENTRY el0_sync /* +0x480: IRQ */
    VENTRY hang     /* +0x500: FIQ */
    VENTRY hang     /* +0x580: SError */

    /* GRUPO 4: Lower EL (AArch32) - USER MODE 32-bit */
    VENTRY hang    /* +0x600: Synchronous */
    VENTRY hang    /* +0x680: IRQ */
    VENTRY hang    /* +0x700: FIQ */
    VENTRY hang    /* +0x780: SError */

/* Handler para excepciones no manejadas */
hang:
    b hang