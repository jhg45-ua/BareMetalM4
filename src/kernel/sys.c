/**
 * @file sys.c
 * @brief Implementación de llamadas al sistema (syscalls)
 * 
 * @details
 *   Este archivo contiene:
 *   - Implementación de syscalls básicas (write, exit)
 *   - Dispatcher central para manejo de excepciones síncronas (SVC)
 *   - Handler para fallas de segmentación
 * 
 *   SYSCALLS IMPLEMENTADAS:
 *   - SYS_WRITE (0): Escritura en consola desde procesos de usuario
 *   - SYS_EXIT (1): Terminación de proceso con código de salida
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#include "../../include/kernel/sys.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/kutils.h"

/* ========================================================================== */
/* IMPLEMENTACION DE SYSCALLS                                                */
/* ========================================================================== */

/**
 * @brief Syscall para escritura en consola
 * @param buf Puntero al buffer de caracteres a imprimir
 * 
 * @details
 *   En un SO real, aquí verificaríamos que 'buf' apunta a memoria
 *   válida del usuario antes de acceder.
 */
void sys_write(char *buf) {
    kprintf("%s", buf);
}

/**
 * @brief Syscall para terminar proceso actual
 * @param code Código de salida del proceso
 * 
 * @details
 *   Imprime un mensaje de diagnóstico y llama a exit() para
 *   terminar el proceso actual y marcarlo como zombie.
 */
void sys_exit(int code) {
    kprintf("\n[SYSCALL] Proceso solicitó salida con código %d\n", code);
    exit();
}

/* ========================================================================== */
/* DISPATCHER DE SYSCALLS                                                     */
/* ========================================================================== */

/**
 * @brief Handler central de syscalls (dispatcher)
 * @param regs Puntero a registros guardados del proceso
 * @param syscall Número de syscall solicitada (contenido de x8)
 * 
 * @details
 *   Este handler es llamado desde sync_handler en entry.S cuando
 *   se ejecuta una instrucción SVC. Despacha la syscall apropiada
 *   según el número en el registro x8.
 */
void syscall_handler(struct pt_regs *regs, int syscall) {
    switch (syscall) {
        case SYS_WRITE:
            /* Argumento en x19 según convención actual */
            sys_write((char *)regs->x19);
            break;

        case SYS_EXIT:
            sys_exit((int)regs->x19);
            break;

        default:
            kprintf("Syscall desconocida: %d\n", syscall);
            break;
    }
}

/* ========================================================================== */
/* MANEJO DE FALLAS                                                          */
/* ========================================================================== */

/**
 * @brief Handler para excepciones no controladas
 * 
 * @details
 *   Se ejecuta cuando un proceso causa una excepción síncrona no manejada,
 *   como acceso a memoria inválida (segmentation fault).
 *   Termina el proceso culpable en lugar de colgar el sistema.
 */
void handle_fault(void) {
    kprintf("\n[CPU] CRITICAL: Excepcion no controlada (Segmentation Fault)!\n");
    kprintf("[CPU] Matando al proceso actual por violacion de segmento.\n");
    exit();
}