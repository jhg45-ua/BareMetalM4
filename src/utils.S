/**
 * @file utils.S
 * @brief Funciones utilitarias de bajo nivel (Assembly ARM64)
 * 
 * @details
 *   Este archivo contiene funciones wrapper que abstraen operaciones
 *   de hardware comunes en interfaces simples para usar desde C.
 * 
 *   Incluye:
 *   - Lectura/escritura de memoria (MMIO = Memory-Mapped I/O)
 *   - Acceso a registros del timer del sistema
 *   - Configuración de la tabla de vectores de interrupciones
 *   - Control de apagado del sistema (Semihosting)
 * 
 * @section WHY_ASSEMBLY
 *   ¿POR QUÉ ESTAS FUNCIONES ESTÁN EN ASSEMBLY?
 *   Aunque podrían escribirse en C inline, ponerlas en .S proporciona:
 *   - Control explícito sin que el compilador las optimice incorrectamente
 *   - Documentación clara del mapeo entre funciones C y registros ARM64
 *   - Facilidad para depuración (breakpoints en asm)
 *   - Garantía de que las instrucciones MRS/MSR se ejecutan exactamente
 * 
 * @section CONCEPTS
 *   CONCEPTOS:
 *   - MMIO: Registros de hardware accesibles como direcciones de memoria
 *   - MRS: Move from System Register (leer registros de sistema)
 *   - MSR: Move to System Register (escribir registros de sistema)
 *   - Registros de Timer: CNTFRQ_EL0, CNTP_TVAL_EL0, CNTP_CTL_EL0
 *   - VBAR_EL1: Vector Base Address Register (tabla de excepciones)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

.global put32
.global get32
.global timer_get_freq
.global timer_set_tval
.global timer_set_ctl
.global set_vbar_el1
.global system_off

/* 
 * put32 - Escribir un valor de 32 bits en una dirección de memoria (MMIO)
 * 
 * Parámetros:
 *   x0 = Dirección de memoria
 *   w1 = Valor a escribir (32 bits)
 */
put32:
    str w1, [x0]
    ret

/* 
 * get32 - Leer un valor de 32 bits desde una dirección de memoria (MMIO)
 * 
 * Parámetros:
 *   x0 = Dirección de memoria
 * 
 * Retorna:
 *   w0 = Valor leído (32 bits)
 */
get32:
    ldr w0, [x0]
    ret

/* 
 * timer_get_freq - Leer la frecuencia del timer del sistema (en Hz)
 * 
 * Retorna:
 *   x0 = Frecuencia en Hz (típicamente 19.2 MHz en QEMU)
 */
timer_get_freq:
    mrs x0, CNTFRQ_EL0
    ret

/* 
 * timer_set_tval - Configurar el valor de timeout del timer físico
 * 
 * Parámetros:
 *   x0 = Número de ticks hasta la próxima interrupción
 */
timer_set_tval:
    msr cntp_tval_el0, x0
    ret

/* 
 * timer_set_ctl - Habilitar o deshabilitar el timer físico
 * 
 * Parámetros:
 *   x0 = 1 para habilitar, 0 para deshabilitar
 */
timer_set_ctl:
    msr cntp_ctl_el0, x0
    ret

/* 
 * set_vbar_el1 - Establecer la dirección base de la tabla de vectores
 * 
 * Parámetros:
 *   x0 = Dirección de la tabla de vectores (debe estar alineada a 2KB)
 * 
 * La tabla de vectores define los manejadores de excepciones e interrupciones.
 */
set_vbar_el1:
    msr vbar_el1, x0
    ret

/* Bloque de parámetros para Semihosting */
/* Debe estar alineado a 8 bytes (64 bits) */
.align 3 
semihosting_param_block:
    .quad 0x20026   /* ADP_Stopped_Application (Código mágico de salida limpia) */
    .quad 0         /* Exit Code (0 = Éxito) */

/* 
 * system_off - Apagar el sistema usando Semihosting
 * 
 * Usa la interfaz Semihosting de ARM para indicar a QEMU
 * que termine la ejecución limpiamente.
 * 
 * Operación 0x18 = SYS_EXIT
 * Parámetro: bloque con código de salida 0
 */
system_off:
    /* Operación 0x18 = SYS_EXIT */
    mov x0, #0x18
    
    /* Cargamos la dirección del bloque de parámetros en x1 */
    ldr x1, =semihosting_param_block
    
    /* Disparamos la interrupción de semihosting */
    /* QEMU detectará esto y cerrará la ventana */
    hlt #0xf000
    
    /* Si fallara (no debería), nos colgamos */
    b hang