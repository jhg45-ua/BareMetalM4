/* 
 * =============================================================
 * src/utils.S - Funciones Utilitarias de Bajo Nivel
 * =============================================================
 * 
 * PROPOSITO:
 *   Este archivo contiene funciones wrapper que abstraen operaciones
 *   de hardware comunes en interfaces simples para usar desde C.
 * 
 *   Incluye:
 *   - Lectura/escritura de memoria (MMIO = Memory-Mapped I/O)
 *   - Acceso a registros del timer del sistema
 *   - Configuracion de la tabla de vectores de interrupciones
 * 
 * ¿POR QUE ESTAS FUNCIONES ESTAN EN ASSEMBLY?
 *   Aunque podrian escribirse en C inline, ponerlas en .S proporciona:
 *   - Control explícito sin que el compilador las optimice incorrectamente
 *   - Documentacion clara del mapeo entre funciones C y registros ARM64
 *   - Facilidad para depuración (breakpoints en asm)
 *   - Garantía de que las instrucciones MRS/MSR se ejecutan exactamente
 * 
 * CONCEPTOS:
 *   - MMIO: Registros de hardware accesibles como direcciones de memoria
 *   - MRS: Move from System Register (leer registros de sistema)
 *   - MSR: Move to System Register (escribir registros de sistema)
 *   - Registros de Timer: CNTFRQ_EL0, CNTP_TVAL_EL0, CNTP_CTL_EL0
 *   - VBAR_EL1: Vector Base Address Register (tabla de excepciones)
 * =============================================================
 */

.global put32
.global get32
.global timer_get_freq
.global timer_set_tval
.global timer_set_ctl
.global set_vbar_el1

/* 
 * =============================================================
 * FUNCION: put32
 * =============================================================
 * 
 * PROPOSITO:
 *   Escribir un valor de 32 bits en una direccion de memoria.
 *   Usada para acceder a registros de hardware mapeados en memoria (MMIO).
 * 
 * PROTOTIPO C:
 *   void put32(uint64_t addr, uint32_t value);
 * 
 * PARAMETROS:
 *   x0 = direccion de memoria (64 bits)
 *   w1 = valor a escribir (32 bits)
 * 
 * CASOS DE USO:
 *   - Configurar registros de control del GIC (Generic Interrupt Controller)
 *   - Escribir datos en puertos UART (para imprimir)
 *   - Configurar timers
 *   - Controlar GPIO
 * 
 * EJEMPLO EN C:
 *   put32(0x3f201000, 0x12345678);  // Escribir en UART
 * 
 * INSTRUCCION:
 *   STR = Store Register
 *     Escribe un registro en memoria
 */
put32:
    /*
     * STR w1, [x0]
     *   Escribe el valor de 32 bits en w1 a la direccion en x0
     *   [x0] = direccion (puede ser direccion absoluta o registro MMIO)
     * 
     * NOTA: Usamos 'w1' (32 bits) no 'x1' (64 bits) porque los valores MMIO
     * suelen ser 32 bits. Si escribimos 64 bits podria afectar registros adyacentes.
     */
    str w1, [x0]
    ret

/* 
 * =============================================================
 * FUNCION: get32
 * =============================================================
 * 
 * PROPOSITO:
 *   Leer un valor de 32 bits desde una direccion de memoria.
 *   Usada para leer registros de hardware mapeados en memoria (MMIO).
 * 
 * PROTOTIPO C:
 *   uint32_t get32(uint64_t addr);
 * 
 * PARAMETRO:
 *   x0 = direccion de memoria (64 bits)
 * 
 * RETORNO:
 *   w0 = valor leido (32 bits)
 *   
 *   NOTA: El retorno usa w0 (32 bits), pero como esta en x0,
 *   los bits superiores [63:32] se extienden a 0 (unsigned).
 * 
 * CASOS DE USO:
 *   - Leer el estado de interrupciones del GIC
 *   - Leer valores de sensores
 *   - Verificar si un device esta listo
 * 
 * EJEMPLO EN C:
 *   uint32_t status = get32(0x3f208000);  // Leer status del UART
 * 
 * INSTRUCCION:
 *   LDR = Load Register
 *     Lee un valor de memoria a un registro
 */
get32:
    /*
     * LDR w0, [x0]
     *   Lee un valor de 32 bits desde la direccion en x0
     *   Guarda el resultado en w0 (que es el registro de retorno en ABI)
     * 
     * SECUENCIA:
     *   1. [x0] = Leemos 32 bits de memoria
     *   2. w0 = Resultado (automaticamente es el valor de retorno)
     *   3. ret = Retornamos a quien nos llamo
     */
    ldr w0, [x0]
    ret

/* 
 * =============================================================
 * FUNCION: timer_get_freq
 * =============================================================
 * 
 * PROPOSITO:
 *   Leer la frecuencia del timer del sistema (en Hz).
 *   Necesario para configurar correctamente el valor del timeout.
 * 
 * PROTOTIPO C:
 *   uint64_t timer_get_freq(void);
 * 
 * RETORNO:
 *   x0 = frecuencia del timer en Hz
 *   
 *   Ejemplo: 19.2 MHz (19200000 Hz) en Raspberry Pi
 * 
 * REGISTRO CNTFRQ_EL0:
 *   Counter Frequency Register - El sistema ya tiene pre-configurado
 *   este registro con la frecuencia del timer.
 * 
 * ALGORITMO:
 *   1. Leer el registro CNTFRQ_EL0 (frecuencia pre-configurada por hardware)
 *   2. Retornar el valor en x0 (registro de retorno)
 * 
 * USO TIPICO:
 *   uint64_t freq = timer_get_freq();  // 19200000 Hz
 *   uint64_t ticks = freq * 2;         // Timeout de 2 segundos
 *   timer_set_tval(ticks);
 * 
 * INSTRUCCION:
 *   MRS = Move from System Register
 *     Lee un registro del sistema ARM64
 */
timer_get_freq:
    /*
     * MRS x0, CNTFRQ_EL0
     *   MRS = Move from System Register (leer del sistema)
     *   x0 = destino (registro general de 64 bits)
     *   CNTFRQ_EL0 = Counter Frequency Register (EL0 = Exception Level 0)
     * 
     * Este registro es READ-ONLY y contiene la frecuencia del timer
     * configurada por el bootloader o firmware.
     */
    mrs x0, CNTFRQ_EL0
    ret

/* 
 * =============================================================
 * FUNCION: timer_set_tval
 * =============================================================
 * 
 * PROPOSITO:
 *   Configurar el valor de timeout del timer fisico (physical timer).
 *   Cuando el contador interno decae a 0, el timer genera una IRQ.
 * 
 * PROTOTIPO C:
 *   void timer_set_tval(uint64_t ticks);
 * 
 * PARAMETRO:
 *   x0 = numero de ciclos de timer a esperar antes de generar IRQ
 *   
 *   Ejemplo: Si la frecuencia es 19.2MHz y queremos 1ms:
 *     ticks = 19200 (19200000 Hz / 1000)
 *     timer_set_tval(19200);
 * 
 * REGISTRO CNTP_TVAL_EL0:
 *   Counter-Timer Physical Timer Value Register
 *   
 *   Al escribir aqui:
 *     CNTP_CVAL_EL0 = (current_counter + tval)
 *   
 *   El timer dispara cuando current_counter >= CNTP_CVAL_EL0
 * 
 * FLUJO:
 *   1. Obtener frecuencia: freq = timer_get_freq()
 *   2. Calcular ticks para el delay deseado: ticks = freq * segundos
 *   3. Escribir en TVAL: timer_set_tval(ticks)
 *   4. Habilitar el timer: timer_set_ctl(1)
 *   5. Esperar IRQ o usar WFE
 * 
 * INSTRUCCION:
 *   MSR = Move to System Register
 *     Escribe un registro general en un registro del sistema
 */
timer_set_tval:
    /*
     * MSR cntp_tval_el0, x0
     *   MSR = Move to System Register (escribir en el sistema)
     *   cntp_tval_el0 = Counter-Timer Physical Timer Value Register
     *   x0 = valor a escribir (numero de ciclos)
     * 
     * DIFERENCIA CON STR:
     *   - STR w1, [x0]: Escribe en memoria (MMIO)
     *   - MSR cntp_tval_el0, x0: Escribe en un registro del sistema
     * 
     * Los registros del sistema ARM64 no son accesibles como memoria normal.
     * Solo se pueden leer/escribir con MRS/MSR (instrucciones especiales).
     */
    msr cntp_tval_el0, x0
    ret

/* 
 * =============================================================
 * FUNCION: timer_set_ctl
 * =============================================================
 * 
 * PROPOSITO:
 *   Habilitar o deshabilitar el timer fisico.
 * 
 * PROTOTIPO C:
 *   void timer_set_ctl(uint32_t enable);
 * 
 * PARAMETRO:
 *   x0 = valor de control
 *     Bit 0: ENABLE (1=habilitado, 0=deshabilitado)
 *     Bit 1: IMASK (1=mask la IRQ, 0=permite IRQ)
 *   
 *   Ejemplo:
 *     timer_set_ctl(1);      // Habilitar, permitir IRQ
 *     timer_set_ctl(3);      // Habilitar, pero maskear IRQ (esperar sin interrupcion)
 * 
 * REGISTRO CNTP_CTL_EL0:
 *   Counter-Timer Physical Timer Control Register
 * 
 * FLUJO TIPICO:
 *   timer_set_tval(19200000);  // Timeout de 1 segundo
 *   timer_set_ctl(1);          // Habilitar el timer
 *   // Esperar... el timer contara hacia 0
 *   // Cuando llegue a 0, genera IRQ (si no esta mascarado)
 * 
 * INSTRUCCION:
 *   MSR = Move to System Register
 */
timer_set_ctl:
    /*
     * MSR cntp_ctl_el0, x0
     *   Escribir el valor de control del timer
     */
    msr cntp_ctl_el0, x0
    ret

/* 
 * =============================================================
 * FUNCION: set_vbar_el1
 * =============================================================
 * 
 * PROPOSITO:
 *   Establecer la direccion base de la tabla de vectores de excepciones.
 *   Cuando ocurre una excepcion (IRQ, SVC, etc), el CPU salta a direcciones
 *   relativas a VBAR_EL1.
 * 
 * PROTOTIPO C:
 *   void set_vbar_el1(uint64_t vbar_addr);
 * 
 * PARAMETRO:
 *   x0 = direccion base de la tabla de vectores
 *   
 *   Ejemplo: set_vbar_el1(0x80000);  // Tabla de vectores a 0x80000
 * 
 * REGISTRO VBAR_EL1:
 *   Vector Base Address Register - Exception Level 1
 * 
 * TABLA DE VECTORES (ejemplo, 16 entradas de 128 bytes cada una):
 *   
 *   VBAR_EL1 + 0x000:   Synchronous (EL1)
 *   VBAR_EL1 + 0x080:   IRQ (EL1)
 *   VBAR_EL1 + 0x100:   FIQ (EL1)
 *   VBAR_EL1 + 0x180:   SError (EL1)
 *   
 *   VBAR_EL1 + 0x200:   Synchronous (EL0)
 *   VBAR_EL1 + 0x280:   IRQ (EL0)
 *   VBAR_EL1 + 0x300:   FIQ (EL0)
 *   VBAR_EL1 + 0x380:   SError (EL0)
 *   
 *   ...y mas para cambios de modo
 * 
 * FLUJO DE EXCEPCION:
 *   1. Ocurre evento (IRQ, excepciones, etc)
 *   2. CPU salta a VBAR_EL1 + offset (segun el tipo de evento)
 *   3. El codigo en ese offset maneja la excepcion
 *   4. Usa ERET para retornar al codigo interrumpido
 * 
 * EN NUESTRO KERNEL:
 *   Este registro se configura en timer.c:
 *   
 *     extern void set_vbar_el1(uint64_t addr);
 *     set_vbar_el1(&vectors);  // Apuntar a nuestra tabla de vectores
 * 
 *   La tabla de vectores esta definida en vectors.S
 * 
 * INSTRUCCION:
 *   MSR = Move to System Register
 */
set_vbar_el1:
    /*
     * MSR vbar_el1, x0
     *   Escribe la direccion base de la tabla de vectores
     *   Todos los saltos de excepcion seran relativos a esta direccion
     */
    msr vbar_el1, x0
    ret