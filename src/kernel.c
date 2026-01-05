/* src/kernel.c */
#include "../include/types.h"
#include "../include/sched.h"
#include "../include/io.h"
#include "../include/timer.h"
#include "../include/semaphore.h"

/* --- Herramientas Base del Kernel --- */

/* Kernel Panic: Detiene el sistema si algo grave pasa */
void panic(const char *msg) {
    kprintf("\n!!!! KERNEL PANIC !!!!\n");
    kprintf(msg);
    kprintf("\nSistema detenido");
    while(1);
}

/* Un retardo simple para que podamos ver el texto despacio */
/* cada iteracion puede durar varios ciclos de CPU, lo que en el caso de 1000000000 iteraciones puede ser aproximadamente 1 segundo */
void delay(int count) { for (volatile int i = 0; i < count; i++); }

/* --- GESTION DE PROCESOS (Mejorado) --- */
struct pcb process[MAX_PROCESS];    // Array de procesos
struct pcb *current_process;        // Proceso actual
int num_process = 0;

/* Memoria para pilas de procesos (64 procesos x 4KB) 
    En un SO real usariamos 'malloc' o un Page Allocator */
uint8_t process_stack[MAX_PROCESS][4096] __attribute__((aligned(16)));

/* Funcion externa en ensamblador */
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);
extern void timer_init(void);
extern void enable_interrupts(void);

/* Crea un nuevo hilo del kernel */
int create_thread(void (*fn)(void), int priority) {
    if (num_process >= MAX_PROCESS) return -1;
    struct pcb *p = &process[num_process];

    p->pid = num_process;
    p->state = PROCESS_READY;
    p->prempt_count = 0;
    p->priority = priority;

    p->context.x19 = 0;
    p->context.pc = (unsigned long)fn;
    /* La pila crece hacia abajo, apuntamos al final del bloque de 4KB */
    p->context.sp = (unsigned long)&process_stack[num_process][4096];

    num_process++;
    return p->pid;
}

/* Funcion para ceder el turno (Planificador Round Robin con Gestion de Estados) */
void schedule() {
    /* 1. Fase de Envejecimiento */
    /* Recorremos todos los procesos para 'premiar' a los que esperan */
    for (int i = 0; i < num_process; i++) {
        /* Si el proceso esta listo y  NO es el que se esta ejecutando actualmente */
        if (process[i].state == PROCESS_READY && i != current_process->pid) {
            /* ... le subimos la prioridad (restamos un valor) para que sea mas importante */
            /* Ponemos un limite (0) para no tener prioridades negativas */
            if (process[i].priority > 0) {
                process[i].priority--;
            }
        }
    }

    /* 2. Fase de Eleccion (Como antes, busca la mejor prioridad)*/
    int next_pid = -1;
    int highest_priority_found = 1000; // Un numero muy alto (peor prioridad)

    /* Estrategia: Buscar el proceso READY con la MENOR variable 'priority' (Mas Importante) */
    for (int i = 0; i < num_process; i++) {
        /* Solo miramos los procesos Listos o Corriendo */
        if (process[i].state == PROCESS_READY || process[i].state == PROCESS_RUNNING) {
            /* Si encontramos uno con mejor prioridad, es el candidato */
            if (process[i].priority < highest_priority_found) {
                highest_priority_found = process[i].priority;
                next_pid = i;
            }
            /*
                Nota: Si tiene la MISMA prioridad, podriamos hacer un Round Robin entre ellos,
                pero para simplificar hacemos FIFO
            */
        }
    }

    /* Si no hay nadie listo (solo pasa si todos estan bloqueados o zombis) */
    if (next_pid == -1) return;

    struct pcb *next = &process[next_pid];
    struct pcb *prev = current_process;

    /* 
        TRUCO EXTRA: Si elegimos una tarea que había subido mucho por Aging,
        deberíamos restaurar su prioridad original para que no se quede
        siendo la jefa para siempre. 
        Para este ejemplo simple, vamos a penalizarla un poco al ejecutarse. 
    */
    if (next->priority < 10) {
        next->priority += 2;
    }

    if (prev != next) {
        if (prev->state == PROCESS_RUNNING) prev->state = PROCESS_READY;
        next->state = PROCESS_RUNNING;
        current_process = next;

        /* Debug: Ver el cambio de contexto */
        // kprintf("--- Switch: P%d (Prior %d) -> P%d (Prior %d) ---\n", 
        //        prev->pid, prev->priority, next->pid, next->priority);

        cpu_switch_to(prev, next);
    }
}

/* --- Multitarea Expropiativa --- */
void proceso_1() {
    enable_interrupts(); /* ¡Vital! */
    int c = 0;
    while(1) {
        kprintf("[P1] Proceso 1 (Cuenta: %d)\n", c++);
        delay(100000000); 
    }
}

void proceso_2() {
    enable_interrupts(); /* ¡Vital! */
    int c = 0;
    while(1) {
        kprintf("     [P2] Proceso 2 (Cuenta: %d)\n", c++);
        delay(100000000); 
    }
}

/* Punto de entrada principal */
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

    create_thread(proceso_1, 1); // Prioridad media
    create_thread(proceso_2, 1); // Prioridad media

    timer_init(); /* Inicializamos el timer para multitarea expropiativa */

    kprintf("Lanzando Scheduler...\n");
    while(1) {
        asm volatile("wfi"); /* Esperar Interrupcion */
    }
}