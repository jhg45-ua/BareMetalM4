/* 
 * =============================================================
 * src/entry.S - Context Switching y Manejo de Interrupciones
 * =============================================================
 * 
 * PROPOSITO:
 *   Este archivo contiene las funciones "magicas" que permiten la multitarea:
 *   
 *   1. cpu_switch_to()     - Cambia de un proceso a otro
 *   2. irq_handler_stub()  - Maneja interrupciones del timer
 *   3. enable_interrupts() - Activa las IRQs (DAIF register)
 * 
 * CONTEXTO EDUCATIVO:
 *   La multitarea es una "ilusion". La CPU solo ejecuta 1 proceso a la vez,
 *   pero cambia tan rapido entre ellos que parece que corren en paralelo.
 *   
 *   Este archivo implementa el "truco" principal: guardar completamente el
 *   estado de un proceso (registros, stack) y restaurar el de otro.
 * 
 * CONCEPTOS CLAVE:
 *   - Callee-saved registers: x19-x30, SP (la funcion debe preservarlos)
 *   - Caller-saved registers: x0-x18 (el que llama asume que se pierden)
 *   - AAPCS64: ARM Architecture Procedure Call Standard
 * =============================================================
 */

.global cpu_switch_to
.global irq_handler_stub
.global enable_interrupts

/* 
 * =============================================================
 * FUNCION: cpu_switch_to
 * =============================================================
 * 
 * PROPOSITO:
 *   Realiza un cambio de contexto voluntario (context switch cooperativo).
 *   Congela el proceso actual y descongela otro proceso.
 * 
 * PROTOTIPO C:
 *   void cpu_switch_to(struct pcb *prev, struct pcb *next);
 * 
 * PARAMETROS:
 *   x0 = puntero al proceso que dejamos (prev)
 *   x1 = puntero al proceso al que vamos (next)
 * 
 * ESTRUCTURA PCB (Process Control Block):
 *   Offset 0-15:   x19, x20
 *   Offset 16-31:  x21, x22
 *   Offset 32-47:  x23, x24
 *   Offset 48-63:  x25, x26
 *   Offset 64-79:  x27, x28
 *   Offset 80-95:  x29 (FP), x30 (LR)
 *   Offset 96:     SP
 * 
 * POR QUE SOLO GUARDAMOS X19-X30 Y NO X0-X18:
 *   Segun AAPCS64, cuando llamamos a cpu_switch_to() desde schedule():
 *   
 *   - x0-x18 son "caller-saved" → schedule() ya los guardo (si los necesita)
 *   - x19-x30 son "callee-saved" → cpu_switch_to() DEBE preservarlos
 *   
 *   Al hacer context switch, estamos "dentro de una funcion" (schedule).
 *   Solo necesitamos preservar lo que una funcion normal preservaria.
 * 
 * EL TRUCO MAGICO DEL LR (x30):
 *   Cuando guardamos x30 de 'prev', capturamos su "direccion de retorno".
 *   Cuando restauramos x30 de 'next', cargamos SU direccion de retorno.
 *   Al hacer 'ret', saltamos magicamente a donde 'next' estaba esperando!
 * 
 *   Ejemplo visual:
 *     Proceso A llama schedule() → su LR apunta a A_code+4
 *     Guardamos LR de A en su PCB
 *     Cargamos LR de B desde su PCB → LR ahora apunta a B_code+8
 *     Hacemos 'ret' → ¡Saltamos a B_code+8!
 * =============================================================
 */
cpu_switch_to:
    /* 
     * =============================================================
     * FASE 1: GUARDAR el contexto de 'prev' (el proceso saliente)
     * =============================================================
     * 
     * Vamos a "fotografiar" el estado completo del proceso actual para
     * poder "reanudarlo" exactamente donde lo dejamos.
     * 
     * STP = Store Pair
     *   Guarda 2 registros consecutivos en memoria
     *   Sintaxis: stp Rt1, Rt2, [base, #offset]
     */
    
    /* Guardar x19-x20 en offset 0 del PCB */
    stp x19, x20, [x0, #0]
    
    /* Guardar x21-x22 en offset 16 (cada registro = 8 bytes, 2 registros = 16 bytes) */
    stp x21, x22, [x0, #16]
    
    /* Guardar x23-x24 en offset 32 */
    stp x23, x24, [x0, #32]
    
    /* Guardar x25-x26 en offset 48 */
    stp x25, x26, [x0, #48]
    
    /* Guardar x27-x28 en offset 64 */
    stp x27, x28, [x0, #64]
    
    /* 
     * Guardar x29 (Frame Pointer) y x30 (Link Register) en offset 80
     * 
     * x29 = FP = Frame Pointer
     *   Apunta a la base del stack frame actual (para debugging)
     * 
     * x30 = LR = Link Register
     *   ¡REGISTRO MAS IMPORTANTE!
     *   Contiene la direccion de retorno. Cuando hagamos 'ret' al final,
     *   saltaremos a la direccion guardada en x30.
     */
    stp x29, x30, [x0, #80]

    /* 
     * NOTA IMPORTANTE: STP y LDP trabajan con PARES de registros
     * Por eso los offsets son multiplos de 16 bytes (8 bytes * 2 registros)
     */

    /*
     * Guardar el Stack Pointer (SP)
     * 
     * No podemos hacer STP con SP directamente, asi que:
     *   1. Copiamos SP a x9 (registro temporal)
     *   2. Guardamos x9 en el PCB
     */
    mov x9, sp
    str x9, [x0, #96]

    /* 
     * =============================================================
     * FASE 2: CARGAR el contexto de 'next' (el proceso entrante)
     * =============================================================
     * 
     * Ahora "descongelamos" el nuevo proceso, cargando todos sus registros
     * desde su PCB. Es como si nunca lo hubieramos pausado.
     * 
     * LDP = Load Pair
     *   Carga 2 registros desde memoria
     */
    
    /* Cargar x19-x20 desde offset 0 del nuevo PCB */
    ldp x19, x20, [x1, #0]
    
    /* Cargar x21-x22 desde offset 16 */
    ldp x21, x22, [x1, #16]
    
    /* Cargar x23-x24 desde offset 32 */
    ldp x23, x24, [x1, #32]
    
    /* Cargar x25-x26 desde offset 48 */
    ldp x25, x26, [x1, #48]
    
    /* Cargar x27-x28 desde offset 64 */
    ldp x27, x28, [x1, #64]
    
    /* 
     * Cargar x29 y x30 desde offset 80
     * 
     * ¡MOMENTO MAGICO!
     * Al cargar x30 del nuevo proceso, estamos cambiando nuestra "direccion
     * de retorno". Cuando ejecutemos 'ret' al final, NO volveremos a donde
     * el proceso actual llamo a cpu_switch_to(), sino a donde el NUEVO
     * proceso estaba esperando!
     */
    ldp x29, x30, [x1, #80]

    /*
     * Restaurar el Stack Pointer del nuevo proceso
     * 
     * Ahora SP apunta al stack del nuevo proceso. Cualquier PUSH/POP que
     * hagamos afectara al stack de 'next', no al de 'prev'.
     */
    ldr x9, [x1, #96]
    mov sp, x9

    /* 
     * =============================================================
     * FASE 3: SALTAR al nuevo proceso
     * =============================================================
     * 
     * RET = Return from function
     *   Equivale a: PC = x30 (salta a la direccion en Link Register)
     * 
     * MAGIA PURA:
     *   - Si 'next' es un proceso nuevo, x30 apunta a su funcion inicial
     *   - Si 'next' estaba esperando en schedule(), x30 apunta ahi
     *   - En ambos casos, al hacer 'ret', el proceso 'next' continua ejecutando
     *     como si nunca hubiera sido interrumpido
     * 
     * RESULTADO:
     *   El scheduler (quien llamo a cpu_switch_to) cree que la funcion retorno,
     *   pero en realidad saltamos a un proceso completamente distinto.
     *   ¡Es un salto cuantico a otra realidad!
     */
    ret

/* 
 * =============================================================
 * FUNCION: enable_interrupts
 * =============================================================
 * 
 * PROPOSITO:
 *   Habilitar las interrupciones IRQ para que el timer pueda interrumpir
 *   la ejecucion normal del codigo.
 * 
 * REGISTRO DAIF:
 *   Sistema de "mascaras" que controla que interrupciones estan activas:
 *   
 *   Bit 3 (D): Debug exceptions
 *   Bit 2 (A): SError (System Error)
 *   Bit 1 (I): IRQ (Interrupt Request) ← Lo que nos interesa
 *   Bit 0 (F): FIQ (Fast Interrupt Request)
 *   
 *   1 = Deshabilitado (masked)
 *   0 = Habilitado (unmasked)
 * 
 * INSTRUCCION:
 *   MSR daifclr, #2
 *     MSR = Move to System Register
 *     daifclr = DAIF Clear (pone bits a 0)
 *     #2 = binario 0b0010 = bit 1 = IRQ
 *   
 *   Resultado: El bit I se pone a 0 → IRQs habilitadas
 * 
 * PELIGRO:
 *   Despues de esta instruccion, el timer puede interrumpir CUALQUIER
 *   instruccion del codigo. Por eso debemos tener las rutinas de manejo
 *   (irq_handler_stub) listas ANTES de llamar enable_interrupts().
 * =============================================================
 */
enable_interrupts:
    msr daifclr, #2    // Limpiar el bit I (IRQ) del registro DAIF
    ret                // Retornar a quien llamo esta funcion

/* 
 * =============================================================
 * FUNCION: irq_handler_stub
 * =============================================================
 * 
 * PROPOSITO:
 *   Punto de entrada cuando ocurre una interrupcion IRQ del timer.
 *   Este codigo se ejecuta AUTOMATICAMENTE cuando el hardware dispara
 *   una IRQ (configurado via VBAR_EL1 en vectors.S).
 * 
 * FLUJO:
 *   1. Guardar TODO el estado del proceso interrumpido
 *   2. Llamar a handle_timer_irq() en C
 *   3. Restaurar TODO el estado
 *   4. Volver al proceso (posiblemente otro si schedule() cambio de contexto)
 * 
 * DIFERENCIA CON cpu_switch_to:
 *   - cpu_switch_to: Cambio VOLUNTARIO, solo guarda callee-saved (x19-x30)
 *   - irq_handler_stub: Interrupcion INVOLUNTARIA, guarda TODO (x0-x30)
 * 
 * POR QUE GUARDAR TODO:
 *   Una interrupcion puede ocurrir en CUALQUIER momento:
 *   - En medio de una funcion (x0-x18 pueden estar en uso)
 *   - Entre instrucciones de una operacion atomica
 *   - Dentro de un loop critico
 *   
 *   Si no guardamos x0-x18, el proceso reanudaria con registros corruptos.
 * 
 * REGISTROS DE ESTADO:
 *   - SPSR_EL1: Saved Program Status Register
 *     Guarda el estado del procesador (flags, modo, etc) antes de la IRQ
 *   
 *   - ELR_EL1: Exception Link Register
 *     Guarda la direccion de la instruccion que fue interrumpida
 *     Al hacer ERET, saltamos de vuelta a ELR_EL1
 * 
 * CALCULO DE MEMORIA:
 *   32 registros (x0-x30 + LR) + 2 registros de estado = 34 registros
 *   Pero guardamos en pares (16 bytes cada par) = 17 pares = 272 bytes
 *   Redondeamos a 256 bytes para alineacion (suficiente para 32 registros)
 * =============================================================
 */
irq_handler_stub:
    /* 
     * =============================================================
     * FASE 1: GUARDAR TODO el contexto del proceso interrumpido
     * =============================================================
     * 
     * Reservar espacio en el stack: 256 bytes (32 registros * 8 bytes)
     */
    sub sp, sp, #256
    
    /*
     * Guardar TODOS los registros de proposito general (x0-x29)
     * 
     * NOTA: Guardamos en pares con STP para eficiencia
     * Cada par ocupa 16 bytes (2 registros * 8 bytes cada uno)
     */
    stp x0, x1, [sp, #16 * 0]    // Offset 0: x0, x1
    stp x2, x3, [sp, #16 * 1]    // Offset 16: x2, x3
    stp x4, x5, [sp, #16 * 2]    // Offset 32: x4, x5
    stp x6, x7, [sp, #16 * 3]    // Offset 48: x6, x7
    stp x8, x9, [sp, #16 * 4]    // Offset 64: x8, x9
    stp x10, x11, [sp, #16 * 5]  // Offset 80: x10, x11
    stp x12, x13, [sp, #16 * 6]  // Offset 96: x12, x13
    stp x14, x15, [sp, #16 * 7]  // Offset 112: x14, x15
    stp x16, x17, [sp, #16 * 8]  // Offset 128: x16, x17
    stp x18, x19, [sp, #16 * 9]  // Offset 144: x18, x19
    stp x20, x21, [sp, #16 * 10] // Offset 160: x20, x21
    stp x22, x23, [sp, #16 * 11] // Offset 176: x22, x23
    stp x24, x25, [sp, #16 * 12] // Offset 192: x24, x25
    stp x26, x27, [sp, #16 * 13] // Offset 208: x26, x27
    stp x28, x29, [sp, #16 * 14] // Offset 224: x28, x29 (FP)
    
    /* 
     * Guardar registros de estado de la excepcion
     * 
     * MRS = Move from System Register
     *   Lee un registro del sistema y lo copia a un registro general
     * 
     * x21 = SPSR_EL1 (Saved Program Status Register)
     *   Contiene: flags (NZCV), nivel de excepcion, mascaras de interrupcion
     * 
     * x22 = ELR_EL1 (Exception Link Register)
     *   Direccion de la instruccion que estaba a punto de ejecutarse
     *   cuando llego la interrupcion
     */
    mrs x21, spsr_el1
    mrs x22, elr_el1
    
    /* Guardar x30 (LR actual) y SPSR en offset 240 */
    stp x30, x21, [sp, #16 * 15]
    
    /* Guardar ELR en offset 256 (ultimo slot) */
    str x22, [sp, #16 * 16]

    /* 
     * =============================================================
     * FASE 2: Llamar al handler en C
     * =============================================================
     * 
     * BL = Branch with Link
     *   Guarda PC+4 en x30 y salta a handle_timer_irq
     * 
     * handle_timer_irq() esta en timer.c y puede:
     *   - Reconocer la interrupcion en el GIC
     *   - Llamar a schedule() para cambiar de proceso
     *   - Actualizar contadores del sistema
     * 
     * IMPORTANTE:
     *   Si handle_timer_irq() llama a schedule(), este puede cambiar
     *   current_task. Al retornar aqui, restauraremos el contexto del
     *   NUEVO proceso (no necesariamente el que fue interrumpido).
     */
    bl handle_timer_irq

    /* 
     * =============================================================
     * FASE 3: RESTAURAR el contexto (posiblemente de otro proceso)
     * =============================================================
     * 
     * Si schedule() cambio current_task, estamos restaurando el contexto
     * de un proceso DIFERENTE al que fue interrumpido. ¡Planificacion
     * expropiativa en accion!
     */
    
    /* Restaurar ELR_EL1 (direccion de retorno) */
    ldr x22, [sp, #16 * 16]
    
    /* Restaurar x30 (LR) y SPSR_EL1 */
    ldp x30, x21, [sp, #16 * 15]
    
    /*
     * MSR = Move to System Register
     *   Escribe un registro general en un registro del sistema
     * 
     * Restaurar los registros de estado de la excepcion
     */
    msr elr_el1, x22   // Direccion donde reanudar la ejecucion
    msr spsr_el1, x21  // Estado del procesador a restaurar

    /*
     * Restaurar TODOS los registros de proposito general
     * Orden inverso al guardado
     */
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
    
    /* Liberar el espacio del stack reservado al inicio */
    add sp, sp, #256
    
    /* 
     * =============================================================
     * FASE 4: RETORNAR de la excepcion
     * =============================================================
     * 
     * ERET = Exception Return
     *   Instruccion especial que:
     *   1. Restaura SPSR_EL1 → CPSR (flags y estado del procesador)
     *   2. Salta a ELR_EL1 (direccion donde fue interrumpido el proceso)
     *   3. Cambia el nivel de excepcion si es necesario
     * 
     * RESULTADO:
     *   El proceso (sea el original o uno nuevo si schedule() cambio)
     *   continua ejecutando como si la interrupcion nunca hubiera ocurrido.
     *   
     * NOTA MAGICA:
     *   Si schedule() eligio otro proceso, ELR_EL1 ahora apunta al codigo
     *   de ESE proceso. ¡La interrupcion se convirtio en un cambio de contexto!
     */
    eret