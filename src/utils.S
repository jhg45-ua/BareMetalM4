/* 
 * src/utils.S - Funciones Utilitarias de Bajo Nivel
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
 */

.global put32
.global get32
.global timer_get_freq
.global timer_set_tval
.global timer_set_ctl
.global set_vbar_el1
.global system_off

/* Escribir un valor de 32 bits en una direccion de memoria (MMIO) */
put32:
    str w1, [x0]
    ret

/* Leer un valor de 32 bits desde una direccion de memoria (MMIO) */
get32:
    ldr w0, [x0]
    ret

/* Leer la frecuencia del timer del sistema (en Hz) */
timer_get_freq:
    mrs x0, CNTFRQ_EL0
    ret

/* Configurar el valor de timeout del timer fisico */
timer_set_tval:
    msr cntp_tval_el0, x0
    ret

/* Habilitar o deshabilitar el timer fisico */
timer_set_ctl:
    msr cntp_ctl_el0, x0
    ret

/* Establecer la direccion base de la tabla de vectores de excepciones */
set_vbar_el1:
    msr vbar_el1, x0
    ret

/* Función para apagar el sistema usando PSCI */
system_off:
    /* Cargamos el ID de la función SYSTEM_OFF de PSCI (0x84000008) */
    ldr x0, =0x84000008
    
    /* SMC (Secure Monitor Call): Llamamos al firmware/hypervisor */
    smc #0
    
    /* Si por alguna razón fallara, entramos en bucle infinito */
    b hang