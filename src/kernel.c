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
extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);

/* Variables compartidas */
volatile int lock = 0;
volatile int shared_counter = 0;

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

/* Codigo del proceso 1 - Intenta entrar en la seccion critica */
void proceso_1() {
    while(1) {
        uart_puts("\n[P1] Intentando coger el cerrojo...\n");

        spin_lock(&lock); // <-- Entrada en la seccion critica 

        uart_puts("[P1] ¡Tengo el cerrojo! Modificando variable...\n");
        int temp = shared_counter;
        delay(1000000000);
        shared_counter = temp + 1;

        uart_puts("[P1] Soltando cerrojo. Contador = ");
        uart_putc(shared_counter + '0'); // Imprimir numero simple(0-9)
        uart_puts("\n");

        spin_unlock(&lock); // <-- Salida de la seccion critica

        schedule(); // Ceder turno
    }
}

/* Codigo del proceso 2 - Tambien intenta entrar en la seccion critica */
void proceso_2() {
    while(1) {
        uart_puts("\n[P2] Intentando coger el cerrojo...\n");

        /* NOTA: En un sistema real con un solo núcleo, si P1 tiene el cerrojo
           y hacemos switch a P2, P2 se quedaría aquí girando para siempre (Deadlock)
           porque P1 no está ejecutándose para soltarlo.
           
           Para la DEMO, vamos a "simular" que el spinlock funciona viendo
           que si está ocupado, no entramos. 
        */

        // Intento manual para no colgar QEMU (trampa educativa):
        if (lock == 1) {
            uart_puts("[P2] Ups!! Esta ocpado. No puedo entrar\n");
        } else {
            spin_lock(&lock);
            uart_puts("[P2] Tengo el cerrojo!! Soy el rey\n");
            shared_counter = 0; // P2 reseata el contador
            spin_unlock(&lock);
        }
        
        delay(1000000000);
        schedule();
    }
}

/* Funcion para inicializar un proceso */
void init_process(struct pcb *process, void (*func)(), uint8_t *stack_top) {
    process->pid = 1 + (unsigned long)stack_top; // Pid simplificado
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