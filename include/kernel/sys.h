#ifndef SYS_H
#define SYS_H

/* Syscall */
#define SYS_WRITE 0
#define SYS_EXIT 1

/* Prototipo del manejador principal (llamado desde Assembly) */
/* struct pt_regs representa los registros guardados en el stack */
struct pt_regs {
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
    unsigned long fp;
    unsigned long sp;
    unsigned long pc;
    unsigned long pstate;
};

void syscall_handler(struct pt_regs *regs, int syscall);

#endif //SYS_H