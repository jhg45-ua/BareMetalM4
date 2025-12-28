#ifndef SCHED_H
#define SCHED_H

#include "types.h"

/* Esta estructura guarda los registros que "sobreviven" a una llamada de funcion 
    (Callee-saved registers) en ARM64. Cuando cambiamos de proceso, guardamos esto 
    para poder volver exactamente al mismo punto despues 
*/
struct cpu_context {
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
    unsigned long fp; // x29 (Frame Pointer)
    unsigned long lr; // x30 (Link Register | Program Counter de retorno)
    unsigned long sp; // Stack Pointer 
};

/* PCB: Process Control Block - La ficha de identidad del proceso */
struct pcb {
    struct cpu_context context;
    long state; // 0 = Ready | 1 = Running, etc...
    long pid;   // Process ID
};

#endif // SCHED_H