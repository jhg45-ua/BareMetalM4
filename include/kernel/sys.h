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
 * @version 0.5
 */

#ifndef SYS_H
#define SYS_H

/* ========================================================================== */
/* NUMEROS DE SYSCALLS                                                       */
/* ========================================================================== */

#define SYS_WRITE 0  /* Escritura en consola */
#define SYS_EXIT  1  /* Terminación de proceso */

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
    unsigned long x19;     /* Registros callee-saved */
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;      /* Frame pointer (x29) */
    unsigned long sp;      /* Stack pointer */
    unsigned long pc;      /* Program counter */
    unsigned long pstate;  /* Processor state */
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