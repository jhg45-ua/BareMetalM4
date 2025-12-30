/* src/kernel.c */
#include "types.h"
#include "sched.h"

/* --- Herramientas Base del Kernel --- */

/* Direccion fisica del UART0 en QEMU 'virt' machine */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;
/* Funcion auxiliar para escribir un caracter  */
void uart_putc(char c) { *UART0_DIR = c; /* Escribir en esta direccion envia el dato a la terminal */ }
/* Funcion auxiliar para escribir una cadena */
void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

/* --- Función para imprimir números (útil para ver los slots) --- */
void uart_print_int(int n) {
    char buffer[10];
    int i = 0;
    if (n == 0) { uart_putc('0'); return; }
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) uart_putc(buffer[--i]);
}

/* Kernel Panic: Detiene el sistema si algo grave pasa */
void panic(const char *msg) {
    uart_puts("\n!!!! KERNEL PANIC !!!!\n");
    uart_puts(msg);
    uart_puts("\nSistema detenido");
    while(1);
}

/* --- GESTION DE PROCESOS (Mejorado) --- */
struct pcb process[MAX_PROCESS];    // Array de procesos
struct pcb *current_process;        // Proceso actual
int num_process = 0;

/* Memoria para pilas de procesos (64 procesos x 4KB) 
    En un SO real usariamos 'malloc' o un Page Allocator */
uint8_t process_stack[MAX_PROCESS][4096]; 

/* Funcion externa en ensamblador */
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);

/* Variables compartidas */
volatile int lock = 0;
volatile int shared_counter = 0;

/* Crea un nuevo hilo del kernel */
int create_thread(void (*fn)(void)) {
    if (num_process >= MAX_PROCESS) return -1;

    struct pcb *p = &process[num_process];
    p->pid = num_process;
    p->state = PROCESS_READY;
    p->prempt_count = 0;

    p->context.x19 = 0;
    p->context.pc = (unsigned long)fn;
    /* La pila crece hacia abajo, apuntamos al final del bloque de 4KB */
    p->context.sp = (unsigned long)&process_stack[num_process][4096];

    num_process++;
    return p->pid;
}

/* Funcion para ceder el turno (Planificador Round Robin con Gestion de Estados) */
void schedule() {
    int current_pid = current_process->pid;
    int next_pid = current_pid; // Por defecto nos quedamos igual si no hay nadie mas

    /* 1. Fase de Busqueda: Solo buscamos quien es el siguiente */
    for (int i = 1; i <= MAX_PROCESS; i++) {
        int candidate_pid = (current_pid + i) % num_process;

        if (process[candidate_pid].state == PROCESS_READY || 
            process[candidate_pid].state == PROCESS_RUNNING) {
                next_pid = candidate_pid;
                break; // Encontrado!!! Salimos del bucle para procesarlo
        }
    }

    /* 2. Fase de Cambio de Contexto: Ejecutamos fuera del bucle */
    struct pcb *next = &process[next_pid];
    struct pcb *prev = current_process;

    /* Si no hay nadie mas listo (salvo el proceso actual) volvemos*/
    if (next->state != PROCESS_READY && next->state != PROCESS_RUNNING) {
        return;
    }

    if (prev != next) {
        /* CORRECCIÓN 2: Usar '=' (asignación) y no '==' (comparación) */
        if (prev->state == PROCESS_RUNNING) {
            prev->state = PROCESS_READY;
        }
        next->state = PROCESS_RUNNING;

        current_process = next;
        cpu_switch_to(prev, next);
    }

}

/* --- Semaforos --- */
struct semaphore {
    volatile int count;
    /*
        En un SO real, aqui iria una lista de espera(wait_queue). Para simplificar, marcaremos procesos como BLOCKED y
        el scheduler los saltara hasta que el semaforo los despierte
    */
};

extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);

/*
    Como no tenemos listas de espera complejas, usaremos un spinlock global para proteger, la operacion
    del semaforo atomicamente
*/
volatile int sem_lock = 0;

void sem_init(struct semaphore *s, int value) {
    s->count = value;
}

/* WAIT [P()]: Intenta entrar, si no puede se duerme */
void sem_wait(struct semaphore *s) {
    while (1) {
        spin_lock(&sem_lock); // Protegemos el acceso

        if (s->count > 0) {
            s->count--;
            spin_unlock(&sem_lock);
            return; // Entramos con exito
        }
        /*
            El recurso esta ocupado: BLOQUEAMOS el proceso actual
            Nota: Esto es insuficiente porque soltariamos el lock y volvemos a intentarlo
            en la siguiente vuelta, pero ilustra el concepto de "no pasar"
        */
        spin_unlock(&sem_lock);

        /* 
            Marcamos el proceso como BLOQUEADO para que el scheduler no lo elija
            NOTA IMPORTANTE: En esta implementación simplificada, necesitamos
            que el sem_post nos despierte.
        */
        
        // Simulación simple de bloqueo: cedemos CPU voluntariamente
        // En un sistema completo: current->state = TASK_BLOCKED;
        schedule();
    }

    
}

/* SIGNAL [V()]: Libera y despierta */
void sem_signal(struct semaphore *s) {
    spin_lock(&sem_lock);
    s->count++;
    spin_unlock(&sem_lock);
    /*
        En un sistema real, aquí buscaríamos un proceso BLOCKED en la lista
        del semáforo y lo pondríamos en READY
    */
}

/* Experimento del Productor-Consumidor */
struct semaphore sem_items; // Cuenta los elementos disponibles
struct semaphore sem_space; // Cuanta los huecos libres

/* El Buffer Real */
int buffer[BUFFER_SIZE];
int in = 0;  // Índice donde escribe el Productor
int out = 0; // Índice donde lee el Consumidor
int next_produced = 1; // Valor a generar (1, 2, 3...)

/* Un retardo simple para que podamos ver el texto despacio */
/* cada iteracion puede durar varios ciclos de CPU, lo que en el caso de 1000000000 iteraciones puede ser aproximadamente 1 segundo */
void delay(int count) { for (volatile int i = 0; i < count; i++); }

void productor() {
    while(1) {
        delay(200000000); // Ajusta este valor según prefieras

        uart_puts("\n[PROD] Generando dato "); 
        uart_print_int(next_produced); 
        uart_puts("...\n");

        sem_wait(&sem_space); /* Esperar hueco (Down Space) */

        /* --- SECCIÓN CRÍTICA (Escribir en Buffer) --- */
        buffer[in] = next_produced;
        uart_puts("[PROD] Puesto en slot [");
        uart_print_int(in);
        uart_puts("].\n");
        
        in = (in + 1) % BUFFER_SIZE; /* Avanzar índice circularmente */
        next_produced++;
        /* ------------------------------------------- */

        sem_signal(&sem_items); /* Avisar que hay un nuevo item (Up Items) */
    }
}

void consumidor() {
    while(1) {
        /* Consumidor más lento para ver cómo se llena el buffer */
        delay(800000000); 

        uart_puts("      [CONS] Comprobando buffer...\n");

        sem_wait(&sem_items); /* Esperar item (Down Items) */

        /* --- SECCIÓN CRÍTICA (Leer de Buffer) --- */
        int dato = buffer[out];
        uart_puts("      [CONS] Leido dato ");
        uart_print_int(dato);
        uart_puts(" del slot [");
        uart_print_int(out);
        uart_puts("].\n");

        out = (out + 1) % BUFFER_SIZE; /* Avanzar índice circularmente */
        /* ---------------------------------------- */

        sem_signal(&sem_space); /* Avisar que hay un hueco libre (Up Space) */
    }
}

/* Punto de entrada principal */
void kernel() {
    uart_puts("¡¡¡Hola desde BareMetalM4!!!\n");
    uart_puts("Sistema Operativo iniciando...\n");

    /* Inicializamos Kernel como Proceso 0 */
    current_process = &process[0];
    current_process->pid = 0;
    current_process->state = PROCESS_RUNNING;
    num_process = 1;

    /* 
        Configuración:
            - 0 items al principio
            - 4 espacios libres (BUFFER_SIZE)
    */
    sem_init(&sem_items, 0);
    sem_init(&sem_space, BUFFER_SIZE);

   /* Creamos los procesos dinamicamente */
   create_thread(productor);
   create_thread(consumidor);

    uart_puts("Lanzando Scheduler...\n");
    while(1) {
        schedule();
    }
}