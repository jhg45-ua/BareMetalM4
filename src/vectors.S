/* 
 * =============================================================
 * src/vectors.S - Tabla de Vectores de Excepciones ARM64
 * =============================================================
 * 
 * PROPOSITO:
 *   Define la tabla de vectores de excepciones (Exception Vector Table).
 *   Es una estructura de bajo nivel que el procesador ARM64 usa para saber
 *   a que direccion saltar cuando ocurre una excepcion (IRQ, SVC, etc).
 * 
 * CONCEPTO CLAVE:
 *   Cuando ocurre una interrupcion o excepcion:
 *   1. El CPU salta a: VBAR_EL1 + offset
 *   2. VBAR_EL1 es establecido por set_vbar_el1() en utils.S
 *   3. El offset depende del tipo de excepcion y del nivel de privilegio
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
 * TIPOS DE EXCEPCIONES:
 *   - Synchronous: Excepciones sincronas (divisor por 0, acceso memoria invalido)
 *   - IRQ: Interrupt Request (timer, I/O devices)
 *   - FIQ: Fast Interrupt Request (no usamos en este kernel)
 *   - SError: System Error (errores criticos de hardware)
 * 
 * FLUJO DE EXCEPCION:
 *   1. Ocurre evento (timer tick, por ejemplo)
 *   2. CPU realiza "exception entry":
 *      - Guarda registro SPSR_EL1 (Program Status)
 *      - Guarda registro ELR_EL1 (exception return address)
 *      - Cambia a exception mode
 *   3. CPU salta a VBAR_EL1 + offset
 *   4. Nuestro codigo en el vector maneja la excepcion
 *   5. ERET retorna al codigo interrumpido
 * 
 * ALINEACION Y ESPACIADO:
 *   - La tabla debe estar alineada a 2KB (.align 11 = 2^11 = 2048 bytes)
 *   - Cada entrada ocupa exactamente 128 bytes (.align 7 = 2^7 = 128 bytes)
 *   - Esto es un requisito hardware ARM64 (no se puede violar)
 * =============================================================
 */

.global vectors
.global irq_handler_stub

/* 
 * MACRO: VENTRY
 * 
 * PROPOSITO:
 *   Macro que crea una entrada de vector de excepcion.
 *   Simplifica la sintaxis y garantiza alineacion correcta.
 * 
 * SINTAXIS:
 *   VENTRY handler_label
 * 
 * LO QUE HACE:
 *   1. .align 7 → Alinea la siguiente instruccion a 128 bytes (2^7)
 *   2. b label → Branch (salto incondicional) a la etiqueta
 * 
 * RAZON DE SER:
 *   Las entradas del vector deben estar exactamente 128 bytes apartadas.
 *   La macro garantiza esto automaticamente en cada entrada.
 * 
 * EJEMPLO:
 *   VENTRY irq_handler_stub
 *   
 *   Se expande a:
 *     .align 7
 *     b irq_handler_stub
 */
.macro VENTRY label
    .align 7
    b \label
.endm

/* 
 * TABLA DE VECTORES
 * 
 * .align 11 → Alinea a 2048 bytes (2^11)
 * Esta es la alineacion requerida por ARM64 para la tabla completa.
 * El registro VBAR_EL1 apuntara exactamente aqui.
 */
.align 11
vectors:
    /* 
     * =============================================================
     * GRUPO 1: Current EL with SP0 (EL1 usando el stack pointer 0)
     * =============================================================
     * 
     * SITUACION:
     *   Cuando el kernel estaba ejecutando ANTES de la excepcion,
     *   estaba usando SP_EL0 (stack pointer del usuario).
     * 
     * ¿CUANDO OCURRE?
     *   Casi nunca en un kernel simple. Solo si el kernel ha
     *   configurado explicitamente el stack para usar SP0.
     *   Generalmente estos vectores nunca se alcanzan.
     * 
     * EN NUESTRO KERNEL:
     *   Como usamos SP_EL1 (stack del kernel), estos no se usan.
     *   Pero deben estar definidos en la tabla (requisito ARM64).
     * 
     * OFFSET DE MEMORIA:
     *   +0x000: Synchronous   (excepciones sincronas)
     *   +0x080: IRQ           (interrupciones de hardware)
     *   +0x100: FIQ           (fast interrupts)
     *   +0x180: SError        (system errors)
     */
    
    /* +0x000: Synchronous exception (SP0) */
    VENTRY hang
    
    /* +0x080: IRQ (SP0) */
    VENTRY hang
    
    /* +0x100: FIQ (SP0) */
    VENTRY hang
    
    /* +0x180: SError (SP0) */
    VENTRY hang

    /* 
     * =============================================================
     * GRUPO 2: Current EL with SPx (EL1h) ← ¡ESTE ES NUESTRO GRUPO!
     * =============================================================
     * 
     * SITUACION:
     *   El kernel estaba ejecutando y uso su propio stack (SP_EL1).
     *   Una excepcion ocurrio en el kernel mismo.
     * 
     * ¿CUANDO OCURRE?
     *   Cuando el kernel recibe una interrupcion de hardware mientras
     *   ejecuta su propio codigo (no codigo de usuario).
     *   Este es el caso normal en nuestro kernel.
     * 
     * EL1h:
     *   - EL1 = Exception Level 1 (kernel mode)
     *   - h = Handler (usa su propio stack, no el del usuario)
     * 
     * EN NUESTRO KERNEL:
     *   El timer interrupt saltara al handler en +0x280 (IRQ).
     *   El scheduler usara este handler para cambiar de proceso.
     * 
     * OFFSET DE MEMORIA:
     *   +0x200: Synchronous (excepciones dentro del kernel)
     *   +0x280: IRQ (interrupciones de timer, dispositivos, etc) ← IMPORTANTE
     *   +0x300: FIQ (fast interrupts, raro)
     *   +0x380: SError (errores de sistema, critico)
     */
    
    /* +0x200: Synchronous exception (EL1, nuestro stack) */
    VENTRY hang
    
    /* 
     * +0x280: IRQ exception (EL1, nuestro stack)
     * 
     * Este es el vector que se ejecutara cuando el timer genere
     * una IRQ. El CPU saltara aqui, que a su vez saltara a
     * irq_handler_stub en entry.S.
     * 
     * FLUJO:
     *   1. Timer dispara IRQ
     *   2. CPU salta a vectors + 0x280 = VENTRY irq_handler_stub
     *   3. Se ejecuta: b irq_handler_stub
     *   4. irq_handler_stub maneja la interrupcion
     *   5. ERET retorna al codigo interrumpido
     */
    VENTRY irq_handler_stub
    
    /* +0x300: FIQ exception (EL1, nuestro stack) */
    VENTRY hang
    
    /* +0x380: SError exception (EL1, nuestro stack) */
    VENTRY hang

    /* 
     * =============================================================
     * GRUPO 3: Lower EL (AArch64)
     * =============================================================
     * 
     * SITUACION:
     *   Una excepcion ocurrio en un proceso de usuario (EL0)
     *   que estaba ejecutando codigo ARM64 (64-bit).
     * 
     * ¿CUANDO OCURRE?
     *   Si los procesos de usuario generan excepciones o interrupciones.
     *   En nuestro kernel, los procesos de usuario tambien corren en
     *   el mismo stack del kernel, asi que este grupo podria no usarse.
     * 
     * OFFSET DE MEMORIA:
     *   +0x400: Synchronous (excepciones de usuario 64-bit)
     *   +0x480: IRQ (interrupciones durante codigo de usuario 64-bit)
     *   +0x500: FIQ
     *   +0x580: SError
     */
    
    /* +0x400: Synchronous exception (Lower EL, AArch64) */
    VENTRY hang
    
    /* +0x480: IRQ (Lower EL, AArch64) */
    VENTRY hang
    
    /* +0x500: FIQ (Lower EL, AArch64) */
    VENTRY hang
    
    /* +0x580: SError (Lower EL, AArch64) */
    VENTRY hang

    /* 
     * =============================================================
     * GRUPO 4: Lower EL (AArch32)
     * =============================================================
     * 
     * SITUACION:
     *   Una excepcion ocurrio en un proceso de usuario (EL0)
     *   que estaba ejecutando codigo ARM de 32-bit.
     * 
     * ¿CUANDO OCURRE?
     *   Si el kernel soporta procesos 32-bit (legacy ARM).
     *   Nuestro kernel es 64-bit puro, asi que esto casi nunca ocurre.
     * 
     * OFFSET DE MEMORIA:
     *   +0x600: Synchronous (excepciones de usuario 32-bit)
     *   +0x680: IRQ
     *   +0x700: FIQ
     *   +0x780: SError
     */
    
    /* +0x600: Synchronous exception (Lower EL, AArch32) */
    VENTRY hang
    
    /* +0x680: IRQ (Lower EL, AArch32) */
    VENTRY hang
    
    /* +0x700: FIQ (Lower EL, AArch32) */
    VENTRY hang
    
    /* +0x780: SError (Lower EL, AArch32) */
    VENTRY hang

/* 
 * HANDLER INFINITO (para excepciones no manejadas)
 * 
 * Si una excepcion ocurre en un vector que apunta a 'hang',
 * el kernel simplemente se detiene en este bucle infinito.
 * 
 * EN UN KERNEL REAL:
 *   Se imprimiria un mensaje de error, se volcaria un core dump,
 *   y posiblemente se reiniciaria el sistema.
 *   
 * AQUI:
 *   Solo nos detenemos. Es suficiente para un kernel educativo.
 */
hang:
    b hang