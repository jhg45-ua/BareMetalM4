/* src/boot.S */

/*
 * =============================================================
 * src/boot.S - Punto de Entrada del Kernel Bare-Metal
 * =============================================================
 * 
 * Proposito:
 *   Este archivo contiene el primer codigo que ejecuta la CPU al iniciar el sistema.
 *   Su trabajo es filtrar cores secundarios y preparar el stack para C.
 *   
 * FLUJO DE EJECUCCION:
 *   1. CPU enciende -> PC apunta a _start (definido en link.ld)
 *   2. Leemos MPIDR_EL1 para identificar el core
 *   3. Si NO somos core 0 -> loop infinito (WFE)
 *   4. Si SI somos core 0 -> configurar el stack y saltar a kernel()
 *
 * CONCEPTOS ARM64:
 *   - MPIDR_EL1: Registro que identifica el core actual
 *   - WFE: "Wait for Event" - bajo consumo hasta que llegue interrupcion
 *   - SP: Stack Pointer - apunta al tope de la pila de ejecuccion
 * =============================================================
*/

.section .text // Punto de entrada para el programa
.global _start // Hacemos _start visible al linker

_start:
    /* 
     * PASO 1: Identificar el Core
     * 
     * En Raspberry Pi 4 por ejemplo, los 4 cores arrancan simultáneamente.
     * Solo queremos que el core 0 ejecute nuestro kernel.
     * 
     * MPIDR_EL1 (Multiprocessor Affinity Register):
     *   Bits [7:0] = Aff0 (Core ID dentro del cluster)
     *   - 0b00 = Core 0
     *   - 0b01 = Core 1
     *   - 0b10 = Core 2
     *   - 0b11 = Core 3
     */
    /* 
     * PASO 1: Comprobar el ID del procesador (MPIDR_EL1)
     * 
     * MRS = Move to Register from System register
     * Leemos MPIDR_EL1 y lo guardamos en x0
     */
    mrs x0, MPIDR_EL1
    
    /*
     * AND con mascara #3 (binario: 0b11)
     * Esto extrae solo los bits [1:0] que contienen el Core ID
     * Resultado en x0:
     *   0 = Core 0 (Master)
     *   1 = Core 1 (Secundario)
     *   2 = Core 2 (Secundario)
     *   3 = Core 3 (Secundario)
     */
    and x0, x0, #3
    
    /*
     * CBZ = Compare and Branch if Zero
     * Si x0 == 0 (somos Core 0) -> salta a 'master'
     * Si x0 != 0 (cores 1,2,3) -> continua a la siguiente instruccion
     */
    cbz x0, master
    
    /*
     * Los cores secundarios llegan aqui
     * Los mandamos a dormir para siempre (hang)
     * 
     * NOTA EDUCATIVA:
     *   En sistemas reales (Linux, FreeRTOS), estos cores se mantienen en
     *   un loop esperando que el OS les asigne trabajo. Aqui los desactivamos
     *   porque nuestro kernel es single-core.
     */
    b hang

master:
    /*
     * PASO 2: Limpiar el registro BSS
     * 
     * ¿QUE ES BSS?
     *   Block Started by Symbol - Seccion de memoria donde el linker coloca
     *   las variables globales NO inicializadas o inicializadas a 0.
     * 
     *   Ejemplo en C:
     *     int contador;           // Va al BSS (no inicializada)
     *     int total = 0;          // Va al BSS (inicializada a 0)
     *     int constante = 42;     // Va al .data (inicializada con valor)
     * 
     * ¿POR QUE LIMPIARLO?
     *   El compilador C asume que todas las variables globales empiezan en 0.
     *   Pero la RAM al encender contiene "basura" (valores aleatorios).
     *   Si no limpiamos BSS, nuestro programa tendria comportamiento impredecible.
     * 
     * IMPLEMENTACION (omitida por simplicidad):
     *   En un kernel real, hariamos:
     *     ldr x0, =__bss_start    // Direccion inicial de BSS (en link.ld)
     *     ldr x1, =__bss_end      // Direccion final de BSS
     *   loop_bss:
     *     str xzr, [x0], #8       // Escribir 0 y avanzar 8 bytes
     *     cmp x0, x1
     *     b.lo loop_bss
     */
    
    /*
     * PASO 3: Configurar el Stack Pointer (SP)
     * 
     * ¿QUE ES EL STACK?
     *   Memoria donde se guardan:
     *   - Variables locales de funciones
     *   - Direcciones de retorno (cuando llamamos a funciones)
     *   - Parametros de funciones (si no caben en registros)
     * 
     * ¿POR QUE CRECE HACIA ABAJO?
     *   Convencion ARM: SP empieza arriba y decrementa con cada PUSH
     *   
     *   Memoria:
     *     0x80000 ← _stack_top (aqui ponemos SP al inicio)
     *     0x7FFF8 ← Primer dato que guardemos (con PUSH)
     *     0x7FFF0 ← Segundo dato
     *       ...
     * 
     * NOTA CRITICA:
     *   Si el stack crece demasiado (stack overflow), empieza a sobrescribir
     *   otras secciones de memoria (codigo, datos). Por eso _stack_top debe
     *   apuntar a una zona segura con suficiente espacio.
     */
    ldr x0, =_stack_top    // Cargar la direccion del tope del stack (definida en link.ld)
    mov sp, x0              // SP = Stack Pointer register

    /*
     * PASO 4: Saltar a codigo C
     * 
     * BL = Branch with Link
     *   - Guarda la direccion de retorno en x30 (LR = Link Register)
     *   - Salta a la etiqueta 'kernel'
     * 
     * A partir de aqui, el codigo C (src/kernel.c) toma el control.
     * La funcion kernel() NUNCA debe retornar (es el entry point del OS).
     */
    bl kernel

    /*
     * PASO 5: Si kernel() retorna (ERROR CATASTROFICO)
     * 
     * Teoricamente kernel() nunca retorna porque es el bucle principal del OS.
     * Si llegamos aqui, algo salio muy mal (panic, excepcion no manejada, etc).
     * Solo nos queda detener el sistema graciosamente.
     */
hang:
    /*
     * WFE = Wait For Event
     * 
     * Esta instruccion pone el core en modo de bajo consumo hasta que ocurra:
     *   - Una interrupcion (IRQ/FIQ)
     *   - Otro core ejecute SEV (Send Event)
     *   - Una excepcion
     * 
     * ¿POR QUE NO USAR UN LOOP VACIO?
     *   Comparacion:
     *     LOOP VACIO:    while(1) { }     → CPU al 100%, alta temperatura
     *     WFE:           while(1) { wfe } → CPU en idle, ahorra bateria
     * 
     * En sistemas embebidos, el consumo energetico es critico.
     * WFE permite "dormir" cores sin apagar completamente el procesador.
     */
    wfe
    
    /*
     * Volver a hang para repetir WFE infinitamente
     * 
     * NOTA: Si una interrupcion despierta al core, WFE retorna y ejecutamos
     * este branch para volver a dormir. Sin el handler de interrupciones,
     * el core simplemente vuelve a WFE inmediatamente.
     */
    b hang
