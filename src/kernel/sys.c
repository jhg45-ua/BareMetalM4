#include "../../include/kernel/sys.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/kutils.h"

/* --- IMPLEMENTACIÓN REAL DE LAS SYSCALLS --- */

void sys_write(char *buf) {
    /* En un SO real, aquí verificaríamos que 'buf' apunta a memoria válida del usuario */
    kprintf("%s", buf);
}

void sys_exit(int code) {
    kprintf("\n[SYSCALL] Proceso solicitó salida con código %d\n", code);
    exit(); /* Llamamos a la función real de process.c */
}

/* --- DISPATCHER (La Centralita) --- */
void syscall_handler(struct pt_regs *regs, int syscall) {
    // kprintf("[SYSCALL] ID: %d\n", syscall_num); // Debug opcional

    switch (syscall) {
        case SYS_WRITE:
            /* Convencion: el argumento 1 está en x0 */
            sys_write((char *)regs->x19);   /* Nota: Ajustaremos qué registro es x0 en entry.S */
            break;

        case SYS_EXIT:
            sys_exit((int)regs->x19);
            break;

        default:
            kprintf("Syscall desconocida: %d\n", syscall);
            break;
    }
}