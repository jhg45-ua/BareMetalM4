/* src/kernel.c */
#include "types.h"
#include "sched.h"

/* Direccion fisica del UART0 en QEMU 'virt' machine */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;

/* Funcion auxiliar para escribir un caracter  */
void uart_putc(char c) {
    *UART0_DIR = c; /* Escribir en esta direccion envia el dato a la terminal */
}

/* Funcion auxiliar para escribir una cadena */
void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        s++;
    }
}

/* --- GESTION DE PROCESOS --- */
/* Definimos 2 procesos + 1 para el kernel inicial */
struct pcb kernel_proccess;
struct pcb process_1;
struct pcb process_2;

/* Pilas para los procesos */
uint8_t stack_1[4096];
uint8_t stack_2[4096];

/* Puntero al proceso actual */
struct pcb *current_process = &kernel_proccess;

/* Funcion externa en ensamblador */
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);

/* Funcion para ceder el turno (Planificador Round Robin muy simple) */
void schedule() {
    struct pcb *prev = current_process;
    struct pcb *next;

    /* Si soy el kernel o P2, le toca a P1. Si soy P1, le toca a P2 */
    if (current_process == &process_1) {
        next = &process_2;
    } else {
        next = &process_1;
    }

    /* Solo cambiamos si es necesario */
    if (prev != next) {
        current_process = next;
        cpu_switch_to(prev, next);
    }
}

/* Un retardo simple para que podamos ver el texto despacio */
/* cada iteracion puede durar varios ciclos de CPU, lo que en el caso de 1000000000 iteraciones puede ser aproximadamente 1 segundo */
void delay(int count) {
    for (volatile int i = 0; i < count; i++);
}

/* Codigo del proceso 1 */
void proceso_1() {
    while(1) {
        // Imprimimos un mensaje de ejemplo realista como: "Proceso 1: ejecutando proceso de gestión de datos"
        uart_puts("Proceso 1: ejecutando proceso de gestión de datos\n");
        delay(1000000000); // Esperar un poco
        schedule();
    }
}

/* Codigo del proceso 2 */
void proceso_2() {
    while(1) {
        uart_puts("Proceso 2: ejecutando proceso de gestión de recursos\n");
        delay(1000000000); // Esperar un poco
        schedule();
    }
}

/* Funcion para inicializar un proceso */
void init_process(struct pcb *process, void (*func)(), uint8_t *stack_top) {
    process->pid = 1; // Pid simplificado
    process->state = 0;

    /* Contexto inicial */
    process->context.x19 = 0;
    process->context.lr = (unsigned long)func; // Cuando se cargue saltara aqui
    process->context.sp = (unsigned long)stack_top; // Su pila empieza aqui
}

/* Punto de entrada principal */
void kernel() {
    uart_puts("¡¡¡Hola desde BareMetalM4!!!\n");
    uart_puts("Sistema Operativo iniciando...\n");

    uart_puts("Inicializando multitarea...\n");

    /* Preparamos las fichas (PCBs) de los procesos */
    /* Nota: stack_1 + 4096 es el final del array (cima de la pila) */
    init_process(&process_1, proceso_1, stack_1 + 4096);
    init_process(&process_2, proceso_2, stack_2 + 4096);

    uart_puts("Lanzando Proceso 1...\n");

    /* Forzamos el primer cambio de contexto manual */
    current_process = &process_1;
    cpu_switch_to(&kernel_proccess, &process_1);


    /* Bucle infinito para que el SO no termine, aqui no llegamos si todo va bien, el control se quedara entre P1 y P2 */
    while(1);
}