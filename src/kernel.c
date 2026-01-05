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

/**
 * @brief Detiene el sistema de emergencia en caso de error critico
 * 
 * @details
 *   Cuando algo catastrofico ocurre:
 *   - Imprime mensaje de error
 *   - Entra en bucle infinito
 *   - Sistema completamente detenido
 * 
 * @param msg Mensaje de error a mostrar (string constante)
 * @return No retorna (bucle infinito)
 * 
 * @note Este es el manejador de ultima oportunidad del kernel
 */
void panic(const char *msg) {
    kprintf("\n!!!! KERNEL PANIC !!!!\n");
    kprintf(msg);
    kprintf("\nSistema detenido");
    while(1);
}

/**
 * @brief Retardo activo (busy-wait) para ralentizar la ejecucion
 * 
 * @details
 *   Ejecuta un bucle vacio 'count' veces. Cada iteracion consume
 *   varios ciclos de CPU. Con volatile impedimos optimizaciones.
 *   
 *   PROPOSITO EDUCATIVO:
 *   - Hacer visible la salida (el kernel no corre tan rapido)
 *   - Dejar tiempo para observar cambios entre procesos
 *   
 *   TIEMPO APROXIMADO (Raspberry Pi 4):
 *   - count = 100,000,000  -> ~0.5 segundos
 *   - count = 1,000,000,000 -> ~5 segundos
 * 
 * @param count Numero de iteraciones del bucle
 * @return void
 * 
 * @warning Bloquea la ejecucion (no se ejecutan otros procesos)
 *          Solo usar en procesos, no en scheduler
 * 
 * @see Se puede reemplazar con timer_wait() para delays precisos
 */
void delay(int count) { for (volatile int i = 0; i < count; i++); }

/* ========================================================================== */
/* GESTION DE PROCESOS - ESTRUCTURAS GLOBALES                               */
/* ========================================================================== */

/**
 * @brief Array de Process Control Blocks para todos los procesos
 * 
 * Cada proceso tiene su PCB en process[i], donde i = PID.
 * Maximo MAX_PROCESS procesos simultaneamente en el sistema.
 */
struct pcb process[MAX_PROCESS];

/**
 * @brief Puntero al proceso actualmente en ejecucion (PROCESS_RUNNING)
 * 
 * El scheduler actualiza este puntero cada vez que ocurre un context switch.
 * El IRQ handler usa este puntero para saber que proceso salvar/restaurar.
 */
struct pcb *current_process;

/**
 * @brief Contador del numero total de procesos creados
 * 
 * Se incrementa cada vez que create_thread() es llamado exitosamente.
 * Cuando num_process == MAX_PROCESS, no se pueden crear mas procesos.
 */
int num_process = 0;

/**
 * @brief Stacks de ejecucion para cada proceso
 * 
 * @details
 *   - MAX_PROCESS procesos x 4096 bytes cada uno = 256KB total
 *   - Alineacion a 16 bytes (requisito AAPCS64 para ARM64)
 *   - Cada proceso[i] usa process_stack[i] como su memoria de pila
 *   
 *   NOTA: El stack crece HACIA ABAJO en ARM64
 *         Si stack[i] empieza en 0x80000, decrementa hacia 0x7F000
 * 
 * @see create_thread() inicializa SP al final del stack (crecimiento descendente)
 */
uint8_t process_stack[MAX_PROCESS][4096] __attribute__((aligned(16)));

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Ensamblador)                                         */
/* ========================================================================== */

/**
 * @brief Context switch entre dos procesos (src/entry.S)
 * 
 * @param prev Puntero al PCB del proceso saliente
 * @param next Puntero al PCB del proceso entrante
 * @return void
 * 
 * @details
 *   Realiza un cambio de contexto atomico:
 *   1. Guarda registros callee-saved de 'prev' en su PCB
 *   2. Carga registros callee-saved de 'next' desde su PCB
 *   3. Restaura SP de 'next'
 *   4. El "truco magico": restaurar x30 (LR) hace que ret
 *      salte al codigo del nuevo proceso
 * 
 * @see src/entry.S para implementacion en assembly
 */
extern void cpu_switch_to(struct pcb *prev, struct pcb *next);

/**
 * @brief Inicializa el timer del sistema (src/timer.c)
 * 
 * @details
 *   Configura:
 *   - VBAR_EL1 (tabla de vectores de excepciones)
 *   - Generic Interrupt Controller (GIC)
 *   - Timer fisico ARM (physical timer)
 *   - Periodo de interrupciones (ej: cada 10ms)
 * 
 * @warning Debe llamarse ANTES de enable_interrupts()
 */
extern void timer_init(void);

/**
 * @brief Habilita las interrupciones IRQ en el procesador
 * 
 * @details
 *   Modifica el registro DAIF (Debug/SError/IRQ/FIQ mask):
 *   - Limpia el bit I (IRQ mask)
 *   - Permite que el timer interrumpa la ejecucion
 *   - Activa multitarea expropiativa
 * 
 * @warning CRITICO: Llamar DESPUES de timer_init()
 *          Si se llama antes, no habria handler para las IRQs
 */
extern void enable_interrupts(void);

/* ========================================================================== */
/* CREACION DE PROCESOS                                                      */
/* ========================================================================== */

/**
 * @brief Crea un nuevo thread (hilo) del kernel
 * 
 * @param fn Puntero a la funcion que ejecutara el thread (sin parametros)
 * @param priority Prioridad inicial (0 = maxima, 255 = minima)
 * 
 * @return PID (Process ID) del nuevo proceso, o -1 si fallo (array lleno)
 * 
 * @details
 *   INICIALIZACION:
 *   - Aloca un nuevo PCB en el array de procesos
 *   - Inicializa: pid, state=READY, priority, registros, stack
 *   - SP apunta al final del stack (crece hacia abajo en ARM64)
 *   - PC apunta a la funcion fn (donde comenzara a ejecutar)
 *   
 *   FLUJO DEL NUEVO PROCESO:
 *   1. Scheduler selecciona el nuevo thread
 *   2. cpu_switch_to() restaura su x30 (Link Register)
 *   3. ret salta a la direccion en x30
 *   4. En este caso, x30 apunta a fn() (la funcion del thread)
 *   5. El thread comienza a ejecutar su codigo
 *   
 *   LIMITES:
 *   - Maximo MAX_PROCESS procesos simultaneamente
 *   - Cada proceso usa 4KB de stack (fijo)
 * 
 * @note El nuevo proceso queda en estado PROCESS_READY,
 *       esperando que el scheduler lo seleccione
 * 
 * @example
 * @code
 * void mi_tarea(void) {
 *     while(1) {
 *         kprintf("Ejecutando mi tarea\\n");
 *         delay(100000000);
 *     }
 * }
 * 
 * // En kernel():
 * int pid = create_thread(mi_tarea, 5);  // prioridad 5
 * @endcode
 * 
 * @see schedule() para ver como se selecciona el siguiente proceso
 * @see struct pcb en include/sched.h para estructura del PCB
 */
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

/* ========================================================================== */
/* SCHEDULER - ALGORITMO DE PRIORIDADES CON AGING                           */
/* ========================================================================== */

/**
 * @brief Planificador de procesos (Scheduler)
 * 
 * @return void
 * 
 * @details
 *   ALGORITMO EN 3 FASES:
 *   
 *   **FASE 1: ENVEJECIMIENTO (AGING)**
 *     Para cada proceso esperando (no ejecutandose):
 *       Si priority > 0: priority--
 *     
 *     PROPOSITO:
 *     - Procesos que esperan mucho tiempo mejoran su prioridad
 *     - Evita "inanicion" (starvation) de tareas de baja prioridad
 *     - Garantiza que eventualmente TODA tarea se ejecute
 *     
 *     MECANISMO:
 *     - Tarea de baja prioridad espera N ciclos
 *     - Su priority baja: 100 -> 99 -> 98 -> ... -> 0
 *     - Eventualmente sera seleccionada
 *   
 *   **FASE 2: SELECCION**
 *     Buscar el proceso READY con MENOR priority:
 *       priority=0   -> MAS IMPORTANTE
 *       priority=255 -> MENOS IMPORTANTE
 *     
 *     Se selecciona el que cumpla:
 *     - Estado es PROCESS_READY o PROCESS_RUNNING
 *     - Numero de priority es el menor de todos
 *   
 *   **FASE 3: EJECUCION Y PENALIZACION**
 *     Si el proceso elegido es diferente al actual:
 *       - Cambiar estado del anterior: RUNNING -> READY
 *       - Cambiar estado del nuevo: READY -> RUNNING
 *       - Aumentar priority del nuevo (+2) para que no monopolice
 *       - Llamar a cpu_switch_to() para hacer el cambio real
 *   
 *   EJEMPLO VISUAL:
 *   @code
 *     Tiempo  P0[P:10] P1[P:20] P2[P:5]  Accion
 *     ------  -------- -------- -------- ------------------
 *     t=1     10       19       4        Aging: P1=19, P2=4
 *     t=2     10       18       3        Aging: P1=18, P2=3
 *     t=3     SWITCH a P2 (priority=3 es minima) P2+=2 -> P2=5
 *     t=4     9        17       5        P0 y P1 envejecen
 *     t=5     SWITCH a P0 (priority=9 es minima) P0+=2 -> P0=11
 *   @endcode
 *   
 *   PREVENSION DE INANICION:
 *     Sin aging:
 *       P0 (priority=1) -> siempre se ejecuta
 *       P1 (priority=100) -> NUNCA se ejecuta (PROBLEMA)
 *     
 *     Con aging:
 *       P1 espera 100 ciclos -> priority baja 100->0 -> se ejecuta
 * 
 * @note Puede llamarse:
 *   - Voluntariamente desde codigo (cooperativo)
 *   - Automaticamente por timer IRQ (expropiativo)
 * 
 * @see cpu_switch_to() para entender el cambio de contexto
 * @see irq_handler_stub() en entry.S (llama a schedule desde IRQ)
 * @see handle_timer_irq() en timer.c (llama a schedule)
 */
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

volatile unsigned long sys_timer_count = 0;

void timer_tick() {
    sys_timer_count++;

    for (int i = 0; i < num_process; i++) {
        if (process[i].state == PROCESS_BLOCKED) {
            if (process[i].wake_up_time <= sys_timer_count) {
                process[i].state = PROCESS_READY;
                kprintf(" [KERNEL] Despertando proceso %d en tick %d\n", i, sys_timer_count);
            }
        }
    }
}

/* ========================================================================== */
/* SLEEP: DORMIR UN PROCESO POR TIEMPO DETERMINADO                         */
/* ========================================================================== */

/**
 * @brief Duerme el proceso actual durante un numero de ticks del timer
 * 
 * @param ticks Numero de interrupciones de timer a esperar (aprox. ~10ms cada una)
 * 
 * @details
 *   FUNCIONAMIENTO:
 *   1. Calcula el \"tiempo de despertar\": wake_up_time = ahora + ticks
 *   2. Marca el proceso como BLOCKED
 *   3. Llama schedule() para ejecutar otro proceso
 *   4. El timer interrupt revisa periodicamente si ya es hora de despertar
 *   5. Cuando wake_up_time <= sys_timer_count, cambia a READY
 *   6. Siguiente schedule() puede elegir este proceso nuevamente
 * 
 *   VENTAJAS RESPECTO A delay():
 *   @code
 *   delay(1000000);    // Quema CPU: bucle vacio, proceso no cede CPU
 *                       // Otros procesos NO pueden ejecutar
 *   
 *   sleep(50);         // Duerme: proceso se bloquea, cede CPU
 *                       // Otros procesos PUEDEN ejecutar
 *   @endcode
 * 
 *   TIEMPO APROXIMADO:
 *   - Cada timer interrupt = ~10ms (TIMER_INTERVAL = 2,000,000 / 19.2MHz)
 *   - sleep(100) = ~1 segundo
 *   - sleep(10) = ~100 milisegundos
 * 
 *   IMPLEMENTACION DETALLES:
 *   - sys_timer_count: Contador global incrementado por handle_timer_irq()
 *   - current_process: Puntero al proceso ejecutandose (actualizado por schedule())\n *   - wake_up_time: Campo en PCB para almacenar momento de despertar
 * 
 *   FLUJO COMPLETO:
 *   @code
 *   Proceso 1 -> sleep(50)
 *       |\\
 *       |  Escribe: wake_up_time = sys_timer_count + 50
 *       |  Escribe: state = BLOCKED
 *       |  Llama: schedule()
 *       |       |\\
 *       |       v  Elige Proceso 2
 *       |  Contexto switch -> Ejecuta Proceso 2
 *       |                       (mientras sys_timer_count incrementa)
 *       |\\
 *       v  Timer interrupt en sys_timer_count = N+50
 *       |  handle_timer_irq() chequea:
 *       |       if (wake_up_time <= sys_timer_count) {
 *       |           state = READY
 *       |       }
 *       |  Siguiente schedule() elige Proceso 1
 *       v  Continua desde donde se durmio
 *   @endcode
 * 
 *   NOTAS SOBRE PRECISION:
 *   - NO es exacta: se revisa cada ~10ms
 *   - El proceso puede dormir mas que lo solicitado
 *     (espera a ser seleccionado nuevamente)
 *   - Para timing critico, usar registros del timer directamente
 *   - Para timing preciso sub-millisegundo, usar busy-wait (delay)
 * 
 *   COMPARACION CON OTROS MECANISMOS:
 *   - sem_wait(): Espera recurso indefinidamente
 *   - sleep(): Espera tiempo determinado
 *   - Condition variable: Espera evento + timeout
 * 
 * @note Proceso debe tener interrupts habilitadas (enable_interrupts())
 *       para que el timer pueda \"despertarlo\"
 * 
 * @warning Bloquea el proceso actual. Si llama sleep() en contexto
 *          de interrupcion o con interrupts deshabilitadas,
 *          nunca se desbloquea.
 * 
 * @see delay() para busy-wait (quema CPU pero synchrono)
 * @see handle_timer_irq() para logica de desbloqueo basada en timer
 * @see schedule() que ejecuta este proceso nuevamente
 * @see sys_timer_count variable global del timer
 */
void sleep(unsigned int ticks) {
    /**
     * PASO 1: Calcular cuando despertar
     * 
     * Sumamos los ticks que esperar al contador actual
     * Si: sys_timer_count = 100, ticks = 50
     * Entonces: wake_up_time = 100 + 50 = 150
     * 
     * El timer interrupt revisara: if (150 <= sys_timer_count)
     * Una vez que sys_timer_count llega a 150, se cambia a READY
     */
    current_process->wake_up_time = sys_timer_count + ticks;

    /**
     * PASO 2: Bloquear el proceso
     * 
     * Cambiar estado a BLOCKED indica que NO puede ejecutar
     * El scheduler saltara este proceso hasta que se cambie a READY
     */
    current_process->state = PROCESS_BLOCKED;

    /**
     * PASO 3: Ceder la CPU
     * 
     * schedule() elegira otro proceso READY para ejecutar
     * Este proceso dormira (no ejecutara mas instrucciones)
     * hasta que handle_timer_irq() lo despierte
     */
    schedule();
}

/* ========================================================================== */
/* PROCESOS DE USUARIO (PROCESOS DE PRUEBA)                                 */
/* ========================================================================== */

/**
 * @brief Primer proceso de usuario (multitarea expropiativa)
 * 
 * @return void
 * 
 * @details
 *   PROPOSITO EDUCATIVO:
 *   - Demuestra como los procesos corren "en paralelo"
 *   - El timer interrupt cambia entre procesos cada ~10ms
 *   - Los "Proceso 1" y "Proceso 2" se intercalan en pantalla
 * 
 *   FLUJO:
 *   1. Habilita interrupciones (CRITICO para que timer pueda interrumpir)
 *   2. Entra en bucle infinito
 *   3. Imprime un contador
 *   4. Hace un delay (ralenta para ver cambios)
 *   5. El timer interrupt lo pausa y ejecuta proceso_2()
 *   6. Cuando se reanuda, continua desde donde se pauso
 * 
 * @warning Si un proceso NO llama enable_interrupts(), 
 *          el timer no podra interrumpirlo
 * 
 * @see proceso_2() para proceso similar
 */
void proceso_1() {
    enable_interrupts(); /* VITAL: Permitir que timer nos interrumpa */
    int c = 0;
    while(1) {
        kprintf("[P1] Proceso 1 (Cuenta: %d)\n", c++);
        /* En vez de quemar CPU con delay_busy, nos echamos una siesta */
        /* sleep(20) hará que el proceso ceda la CPU durante unos ~0.5 segundos */
        sleep(70); 
    }
}

/**
 * @brief Segundo proceso de usuario (multitarea expropiativa)
 * 
 * @return void
 * 
 * @details
 *   Identico a proceso_1() pero con etiqueta diferente para diferenciar.
 *   Los dos procesos corren "simultaneamente" gracias a:
 *   1. Timer que genera IRQ cada ~10ms
 *   2. IRQ handler que llama a schedule()
 *   3. Schedule que cambia entre procesos
 * 
 * @note La salida intercalada [P1]...[P2]...[P1]... demuestra
 *       que el planificador funciona correctamente.
 * 
 * @see proceso_1() para detalles similares
 */
void proceso_2() {
    enable_interrupts(); /* VITAL: Permitir que timer nos interrumpa */
    int c = 0;
    while(1) {
        kprintf("     [P2] Proceso 2 (Cuenta: %d)\n", c++);
        sleep(10);
    }
}

/* ========================================================================== */
/* PUNTO DE ENTRADA DEL KERNEL                                              */
/* ========================================================================== */

/**
 * @brief Punto de entrada principal del kernel
 * 
 * @return void (nunca retorna - bucle infinito esperando interrupciones)
 * 
 * @details
 *   SECUENCIA DE INICIALIZACION:
 *   
 *   1. Imprime banner de bienvenida
 *   2. Inicializa el proceso 0 (kernel) como proceso actual
 *   3. Crea los procesos de usuario (proceso_1, proceso_2)
 *   4. Configura el timer y la tabla de vectores
 *   5. Entra en bucle WFI (Wait For Interrupt)
 *   
 *   DIAGRAMA DE EJECUCION:
 *   @code
 *     kernel() [main]
 *       |
 *       +-- Inicializar proceso 0 (kernel)
 *       |
 *       +-- create_thread(proceso_1, priority=1)
 *       |
 *       +-- create_thread(proceso_2, priority=1)
 *       |
 *       +-- timer_init()
 *       |     (Configura GIC, vectores, timer)
 *       |
 *       +-- WFI loop (Wait For Interrupt)
 *             (Duerme hasta IRQ del timer)
 *   @endcode
 *   
 *   FLUJO REAL DE EJECUCION:
 *   1. kernel() en proceso 0 entra a WFI
 *   2. Timer genera IRQ (cada ~10ms)
 *   3. CPU salta a irq_handler_stub()
 *   4. irq_handler_stub() llama a handle_timer_irq()
 *   5. handle_timer_irq() llama a schedule()
 *   6. schedule() elige el siguiente proceso
 *   7. cpu_switch_to() hace el context switch
 *   8. El nuevo proceso ejecuta su codigo
 *   9. Cuando llega la siguiente IRQ, se repite desde paso 2
 * 
 * @note El bucle WFI (Wait For Interrupt) es eficiente en energia:
 *       - La CPU entra en modo idle (bajo consumo)
 *       - Se despierta solo cuando hay una interrupcion
 *       - Sin WFI, hariamos un loop vacio que consume 100% CPU
 * 
 * @warning Si enable_interrupts() no se llama en los procesos,
 *          el timer no podra interrumpirlos
 * 
 * @see process_1() y proceso_2() para procesos de prueba
 * @see timer_init() para configuracion del timer
 * @see schedule() para logica del planificador
 */
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