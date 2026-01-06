/**
 * @file kernel.c
 * @brief Nucleo principal del Sistema Operativo BareMetalM4
 * 
 * @details
 *   Este archivo contiene la logica central del kernel:
 *   - Manejo de procesos y control de procesos (PCB)
 *   - Scheduler cooperativo/expropiativo con algoritmo de prioridades + aging
 *   - Creacion de threads del kernel
 *   - Funciones de utilidad (panic, delay)
 *   
 * @section ALGORITMO_SCHEDULER
 *   El scheduler implementa un algoritmo de prioridades con "aging":
 *   1. ENVEJECIMIENTO: Para procesos esperando, priority--
 *   2. SELECCION: Buscar el READY con MENOR priority
 *   3. PENALIZACION: Aumentar priority del seleccionado para evitar monopolio
 * 
 * @author Sistema Operativo Educativo
 * @version 0.3
 */

#include "../include/types.h"
#include "../include/sched.h"
#include "../include/io.h"
#include "../include/timer.h"
#include "../include/semaphore.h"

/* ========================================================================== */
/* HERRAMIENTAS BASE DEL KERNEL                                             */
/* ========================================================================== */

/* Detiene el sistema en caso de error critico */
void panic(const char *msg) {
    kprintf("\n!!!! KERNEL PANIC !!!!\n");
    kprintf(msg);
    kprintf("\nSistema detenido");
    while(1);
}

/* Retardo activo (busy-wait) */
void delay(int count) { for (volatile int i = 0; i < count; i++); }

/* ========================================================================== */
/* GESTION DE PROCESOS - ESTRUCTURAS GLOBALES                               */
/* ========================================================================== */

/* Array de Process Control Blocks para todos los procesos */
struct pcb process[MAX_PROCESS];

/* Puntero al proceso actualmente en ejecucion */
struct pcb *current_process;

/* Contador del numero total de procesos creados */
int num_process = 0;

/* Stacks de ejecucion para cada proceso (256KB total) */
uint8_t process_stack[MAX_PROCESS][4096] __attribute__((aligned(16)));

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                         */
/* ========================================================================== */

/* Context switch entre dos procesos (src/entry.S) */
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);

/* Inicializa el timer del sistema (src/timer.c) */
extern void timer_init(void);

/* Habilita las interrupciones IRQ en el procesador */
extern void enable_interrupts(void);

/* Punto de entrada para nuevos procesos (src/entry.S) */
extern void ret_from_fork(void);

/* ========================================================================== */
/* TERMINACION DE PROCESOS                                                   */
/* ========================================================================== */

/* Termina el proceso actual y cede el CPU */
void exit() {
    enable_interrupts();

    kprintf("\n[KERNEL] Proceso %d (%d) ha terminado. Muriendo...\n", 
            current_process->pid, current_process->priority);

    /* Marcar como zombie (PCB persiste hasta implementar wait/reaper) */
    current_process->state = PROCESS_ZOMBIE;

    /* Ceder CPU (schedule() nunca nos volvera a elegir) */
    schedule();
}

/* Hook post-fork (reservado para futuras extensiones) */
void schedule_tail(void) {
    /* Placeholder para logica post-context-switch */
}

/* ========================================================================== */
/* CREACION DE PROCESOS                                                      */
/* ========================================================================== */

/* Crea un nuevo thread (hilo) del kernel */
int create_thread(void (*fn)(void), int priority) {
    if (num_process >= MAX_PROCESS) return -1;
    struct pcb *p = &process[num_process];

    p->pid = num_process;
    p->state = PROCESS_READY;
    p->prempt_count = 0;
    p->priority = priority;

    p->context.x19 = (unsigned long)fn; /* <--- Guardamos la funcion en x19 */
    p->context.pc = (unsigned long)ret_from_fork; /* <--- El PC apunta al wrapper */
    /* La pila crece hacia abajo, apuntamos al final del bloque de 4KB */
    p->context.sp = (unsigned long)&process_stack[num_process][4096];

    num_process++;
    return p->pid;
}

/* ========================================================================== */
/* SCHEDULER - ALGORITMO DE PRIORIDADES CON AGING                           */
/* ========================================================================== */

/* Planificador de procesos (Scheduler) */
void schedule() {
    /* 1. Fase de Envejecimiento */
    for (int i = 0; i < num_process; i++) {
        if (process[i].state == PROCESS_READY && i != current_process->pid) {
            if (process[i].priority > 0) {
                process[i].priority--;
            }
        }
    }

    /* 2. Fase de Eleccion */
    int next_pid = -1;
    int highest_priority_found = 1000;

    for (int i = 0; i < num_process; i++) {
        if (process[i].state == PROCESS_READY || process[i].state == PROCESS_RUNNING) {
            if (process[i].priority < highest_priority_found) {
                highest_priority_found = process[i].priority;
                next_pid = i;
            }
        }
    }

    if (next_pid == -1) return;

    struct pcb *next = &process[next_pid];
    struct pcb *prev = current_process;

    /* Penalizar tarea seleccionada para evitar monopolio */
    if (next->priority < 10) {
        next->priority += 2;
    }

    if (prev != next) {
        if (prev->state == PROCESS_RUNNING) prev->state = PROCESS_READY;
        next->state = PROCESS_RUNNING;
        current_process = next;

        cpu_switch_to(prev, next);
    }
}

/* ========================================================================== */
/* TIMER TICK: ACTUALIZACION DEL SISTEMA DE TIEMPO Y DESPERTAR PROCESOS      */
/* ========================================================================== */

/* Contador global de ticks del sistema */
volatile unsigned long sys_timer_count = 0;

/* Manejador de ticks del timer */
void timer_tick() {
    sys_timer_count++;

    for (int i = 0; i < num_process; i++) {
        if (process[i].state == PROCESS_BLOCKED) {
            if (process[i].wake_up_time <= sys_timer_count) {
                process[i].state = PROCESS_READY;
                // kprintf(" [KERNEL] Despertando proceso %d en tick %d\n", i, sys_timer_count);    // DEBUG
            }
        }
    }
}

/* ========================================================================== */
/* SLEEP: DORMIR UN PROCESO POR TIEMPO DETERMINADO                         */
/* ========================================================================== */

/* Duerme el proceso actual durante un numero de ticks del timer */
void sleep(unsigned int ticks) {
    current_process->wake_up_time = sys_timer_count + ticks;
    current_process->state = PROCESS_BLOCKED;
    schedule();
}

/* Función auxiliar simple para comparar cadenas */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* --- LA TAREA SHELL --- */
void shell_task() {
    enable_interrupts();
    
    char command_buf[64];
    int idx = 0;

    kprintf("\n[SHELL] Bienvenido a BareMetalM4 OS v0.3\n");
    kprintf("[SHELL] Escribe 'help' para ver comandos.\n");
    kprintf("> "); // Prompt

    while (1) {
        /* 1. Intentamos leer una tecla */
        char c = uart_getc_nonblocking();

        if (c == 0) {
            /* Si no hay tecla, dormimos un poco para no quemar CPU */
            /* Esto hace que el shell sea eficiente */
            sleep(1); 
            continue;
        }

        /* 2. Eco local (mostrar lo que escribes) */
        /* Si es Enter (Carriage Return '\r'), procesamos */
        if (c == '\r') {
            uart_putc('\n'); // Salto de línea visual
            command_buf[idx] = '\0'; // Terminador de string

            /* --- EJECUCIÓN DE COMANDOS --- */
            if (strcmp(command_buf, "help") == 0) {
                kprintf("Comandos disponibles:\n");
                kprintf("  help   - Muestra esta ayuda\n");
                kprintf("  ps     - Lista los procesos (simulado)\n");
                kprintf("  clear  - Limpia la pantalla\n");
                kprintf("  panic  - Provoca un Kernel Panic\n");
            } 
            else if (strcmp(command_buf, "ps") == 0) {
                kprintf("PID | Priority | State\n");
                kprintf("----|----------|------\n");
                for(int i=0; i<num_process; i++) {
                     kprintf(" %d  |    %d     |  %d\n", process[i].pid, process[i].priority, process[i].state);
                }
            }
            else if (strcmp(command_buf, "clear") == 0) {
                /* Código ANSI para limpiar terminal */
                kprintf("\033[2J\033[H");
                kprintf("BareMetalM4 Shell\n");
            }
            else if (strcmp(command_buf, "panic") == 0) {
                panic("Usuario solicito panico");
            }
            else if (idx > 0) {
                kprintf("Comando desconocido: %s\n", command_buf);
            }

            /* Resetear buffer y prompt */
            idx = 0;
            kprintf("> ");
        } 
        /* Si es Backspace (borrar) */
        else if (c == 127 || c == '\b') {
            if (idx > 0) {
                idx--;
                uart_puts("\b \b"); // Truco visual para borrar en terminal
            }
        }
        /* Carácter normal */
        else {
            if (idx < 63) {
                command_buf[idx++] = c;
                uart_putc(c); // Eco
            }
        }
    }
}

/* ========================================================================== */
/* PROCESOS DE USUARIO (PROCESOS DE PRUEBA)                                 */
/* ========================================================================== */

/* Primer proceso de usuario (multitarea expropiativa) */
void proceso_1() {
    enable_interrupts();
    int c = 0;
    while(1) {
        kprintf("[P1] Proceso 1 (Cuenta: %d)\n", c++);
        sleep(70); 
    }
}

/* Segundo proceso de usuario (multitarea expropiativa) */
void proceso_2() {
    enable_interrupts();
    int c = 0;
    while(1) {
        kprintf("     [P2] Proceso 2 (Cuenta: %d)\n", c++);
        sleep(10);
    }
}

/* Tarea que cuenta hasta 3 y muere */
void proceso_mortal() {
    enable_interrupts();
    
    for (int i = 0; i < 3; i++) {
        kprintf("     [MORTAL] Vida restante: %d\n", 3 - i);
        sleep(15); /* Duerme un poco */
    }

    kprintf("     [MORTAL] Adios mundo cruel...\n");
    /* AQUÍ OCURRE LA MAGIA:
       Al terminar el for, la función hace 'return'.
       El wrapper 'ret_from_fork' captura ese retorno y llama a 'exit()'. */
}

/* ========================================================================== */
/* PUNTO DE ENTRADA DEL KERNEL                                              */
/* ========================================================================== */

/* Punto de entrada principal del kernel */
void kernel() {
    kprintf("¡¡¡Hola desde BareMetalM4!!!\n");
    kprintf("Sistema Operativo iniciando...\n");
    kprintf("Planificador por Prioridades\n");

    /* Inicializamos Kernel como Proceso 0 */
    current_process = &process[0];
    current_process->pid = 0;
    current_process->state = PROCESS_RUNNING;
    current_process->priority = 20;
    num_process = 1;

    create_thread(shell_task, 1);

    // create_thread(proceso_1, 5);
    // create_thread(proceso_mortal, 5);

    timer_init();

    kprintf("Lanzando Scheduler...\n");
    while(1) {
        asm volatile("wfi");
    }
}