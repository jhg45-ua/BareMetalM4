/**
 * @file sys.h
 * @brief Definiciones para llamadas al sistema (syscalls)
 * 
 * @details
 *   Define:
 *   - Números de syscalls disponibles
 *   - Estructura de registros guardados (pt_regs)
 *   - Interfaz del dispatcher de syscalls
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.6
 */

#ifndef SYS_H
#define SYS_H

/* ========================================================================== */
/* NUMEROS DE SYSCALLS                                                       */
/* ========================================================================== */

#define SYS_WRITE 0  /* Escritura en consola */
#define SYS_EXIT  1  /* Terminación de proceso */
#define SYS_OPEN  2
#define SYS_READ  3

/* ========================================================================== */
/* ESTRUCTURA DE REGISTROS GUARDADOS                                         */
/* ========================================================================== */

/**
 * @struct pt_regs
 * @brief Estructura con registros guardados en el stack
 * 
 * @details
 *   Representa el estado del proceso cuando ocurre una excepción síncrona.
 *   Los registros son guardados por sync_handler en entry.S.
 */
struct pt_regs {
    unsigned long x0;
    unsigned long x1;
    unsigned long x2;
    unsigned long x3;
    unsigned long x4;
    unsigned long x5;
    unsigned long x6;
    unsigned long x7;
    unsigned long x8;
    unsigned long x9;
    unsigned long x10;
    unsigned long x11;
    unsigned long x12;
    unsigned long x13;
    unsigned long x14;
    unsigned long x15;
    unsigned long x16;
    unsigned long x17;
    unsigned long x18;
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long x29;      /* Frame Pointer (FP) */
    unsigned long x30;      /* Link Register (LR) */
    unsigned long pstate;   /* SPSR_EL1 (Estado del procesador) */
    unsigned long pc;       /* ELR_EL1 (Program Counter / Dónde ocurrió la excepción) */
};

/* ========================================================================== */
/* FUNCIONES PUBLICAS                                                         */
/* ========================================================================== */

/**
 * @brief Handler central de syscalls
 * @param regs Puntero a registros guardados del proceso
 * @param syscall Número de syscall solicitada
 * 
 * @details
 *   Llamado desde sync_handler cuando se ejecuta SVC.
 *   Despacha la syscall apropiada según el número en x8.
 */
void syscall_handler(struct pt_regs *regs, int syscall);

/**
 * @brief Handler para fallas de segmentación
 * 
 * @details
 *   Termina el proceso culpable cuando ocurre una
 *   excepción síncrona no manejada.
 */
void handle_fault(void);

#endif /* SYS_H */