/* src/kernel.c */
#include "../include/types.h"
#include "../include/sched.h"
#include "../include/io.h"
#include "../include/timer.h"

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
extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);

/* Variables compartidas */
volatile int lock = 0;
volatile int shared_counter = 0;

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

/* --- Semaforos --- */
struct semaphore {
    volatile int count;
    /*
        En un SO real, aqui iria una lista de espera(wait_queue). Para simplificar, marcaremos procesos como BLOCKED y
        el scheduler los saltara hasta que el semaforo los despierte
    */
};

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

void productor() {
    while (1) {
        delay(200000000); // Ajusta este valor según prefieras

        kprintf("\n[PROD] Generando dato %d...\n", next_produced);

        sem_wait(&sem_space); /* Esperar hueco (Down Space) */

        /* --- SECCIÓN CRÍTICA (Escribir en Buffer) --- */
        buffer[in] = next_produced;
        kprintf("[PROD] Puesto en slot [%d].\n", in);
        
        in = (in + 1) % BUFFER_SIZE; /* Avanzar índice circularmente */
        next_produced++;
        /* ------------------------------------------- */

        sem_signal(&sem_items); /* Avisar que hay un nuevo item (Up Items) */
    }
}

void consumidor() {
    while (1) {
        /* Consumidor más lento para ver cómo se llena el buffer */
        delay(800000000); 

        kprintf("      [CONS] Comprobando buffer...\n");

        sem_wait(&sem_items); /* Esperar item (Down Items) */

        /* --- SECCIÓN CRÍTICA (Leer de Buffer) --- */
        int dato = buffer[out];
        kprintf("      [CONS] Leido dato %d del slot [%d].\n", dato, out);

        out = (out + 1) % BUFFER_SIZE; /* Avanzar índice circularmente */
        /* ---------------------------------------- */

        sem_signal(&sem_space); /* Avisar que hay un hueco libre (Up Space) */
    }
}

/* --- Escenario de Inaniccion --- */
void tarea_tirana() {
    while (1) {
        kprintf("[TIRANA] Soy la jefa (Prior %d). Trabajando...\n", current_process->priority);
        // Delay medio para que de tiempo a ver el texto
        delay(200000000); 
        schedule();
    }
}

void tarea_humilde() {
    while (1) {
        kprintf("       [HUMILDE] Por fin!!! (Prior %d). Gracias....\n", current_process->priority);
        delay(200000000);
        schedule();
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

    /* 
        Configuración:
            - 0 items al principio
            - 4 espacios libres (BUFFER_SIZE)
    */
    // sem_init(&sem_items, 0);
    // sem_init(&sem_space, BUFFER_SIZE);

   /* Creamos los procesos dinamicamente */
    // create_thread(productor);
    // create_thread(consumidor);
    // create_thread(tarea_tirana, 1);  // Prioridad 1 (Muy Alta)
    // create_thread(tarea_humilde, 10); // Prioridad 10 (Muy Baja)

    create_thread(proceso_1, 1); // Prioridad media
    create_thread(proceso_2, 1); // Prioridad media

    timer_init(); /* Inicializamos el timer para multitarea expropiativa */

    kprintf("Lanzando Scheduler...\n");
    while(1) {
        asm volatile("wfi"); /* Esperar Interrupcion */
    }
}