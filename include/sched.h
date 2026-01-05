/**
 * @file sched.h
 * @brief Estructuras y definiciones del sistema de planificacion de procesos
 * 
 * @details
 *   Define:
 *   - Estados posibles de un proceso
 *   - Estructura CPU Context (registros guardados)
 *   - Estructura PCB (Process Control Block)
 *   - Constantes del sistema (max procesos, tamaños, etc)
 * 
 *   El PCB es el registro central de informacion de cada proceso.
 *   Almacena: estado, prioridad, registros guardados, identificadores.
 * 
 * @author Sistema Operativo Educativo
 * @version 1.0
 */

#ifndef SCHED_H
#define SCHED_H

#include "types.h"

/* ========================================================================== */
/* ESTADOS DE PROCESO                                                        */
/* ========================================================================== */

/**
 * @defgroup ProcessStates Estados de Proceso
 * @{
 * 
 * @brief Posibles estados en los que puede estar un proceso
 */

/**
 * @brief Proceso en ejecucion (RUNNING)
 * 
 * El CPU esta ejecutando el codigo de este proceso.
 * Tipicamente hay un solo proceso en este estado a la vez.
 */
#define PROCESS_RUNNING 0

/**
 * @brief Proceso listo para ejecutar (READY)
 * 
 * El proceso esta esperando su turno en la CPU.
 * El scheduler puede seleccionarlo en cualquier momento.
 */
#define PROCESS_READY 1

/**
 * @brief Proceso bloqueado (BLOCKED)
 * 
 * El proceso esta esperando un recurso (semaforo, I/O, etc).
 * El scheduler NO lo seleccionara hasta que sea desbloqueado.
 */
#define PROCESS_BLOCKED 2

/**
 * @brief Proceso terminado (ZOMBIE)
 * 
 * El proceso ha terminado su ejecucion pero su PCB aun existe.
 * Espera a que el padre lo "recolecte" (read exit code, liberar recursos).
 * 
 * En nuestro kernel simplificado, los procesos nunca terminan,
 * asi que este estado se usa raramente.
 */
#define PROCESS_ZOMBIE 3

/** @} */ // Fin ProcessStates

/* ========================================================================== */
/* CONSTANTES DEL SISTEMA                                                    */
/* ========================================================================== */

/**
 * @brief Numero maximo de procesos simultaneamente en el sistema
 * 
 * Limita el array de PCBs. Con 64 procesos y 4KB de stack cada uno,
 * usamos: 64 * sizeof(pcb) + 64 * 4096 = ~256KB de memoria.
 * 
 * Para aumentar: cambiar este define e incrementar process_stack[][]
 */
#define MAX_PROCESS 64

/**
 * @brief Tamaño del buffer para operaciones de I/O o sync
 * 
 * Usado para buffers circulares en semaforos o colas de mensajes.
 * (No usado en esta version simplificada)
 */
#define BUFFER_SIZE 4

/* ========================================================================== */
/* CONTEXTO DE CPU (REGISTROS GUARDADOS)                                    */
/* ========================================================================== */

/**
 * @struct cpu_context
 * @brief Registros ARM64 que se guardan durante context switch
 * 
 * @details
 *   Segun AAPCS64 (ARM Architecture Procedure Call Standard),
 *   los siguientes registros son "callee-saved":
 *   - x19-x28 (registros de proposito general)
 *   - x29 (FP = Frame Pointer)
 *   - x30 (LR = Link Register = direccion de retorno)
 *   
 *   El Stack Pointer (SP) tambien se guarda.
 *   
 *   Cuando se hace context switch:
 *   1. Se guardan TODOS estos registros en el PCB del proceso saliente
 *   2. Se cargan TODOS estos registros desde el PCB del proceso entrante
 *   3. El nuevo proceso continua como si nunca hubiera sido interrumpido
 *   
 *   TAMAÑO:
 *   - 13 registros x 8 bytes cada uno = 104 bytes
 *   - Alineado a 16 bytes = 112 bytes por PCB (aproximadamente)
 * 
 * @see struct pcb para el PCB completo
 * @see cpu_switch_to() en entry.S que usa esta estructura
 */
struct cpu_context {
    /**
     * @brief Registros x19-x28 (callee-saved)
     * 
     * Registros de proposito general que se preservan entre llamadas.
     * Si una funcion los modifica, DEBE restaurarlos antes de retornar.
     */
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
    
    /**
     * @brief fp (Frame Pointer, x29)
     * 
     * Apunta a la base del stack frame actual.
     * Usado para debugging y para mantener la cadena de stack frames.
     * Callee-saved: si se modifica, debe restaurarse.
     */
    unsigned long fp;
    
    /**
     * @brief pc (Program Counter, x30 = Link Register)
     * 
     * Contiene la direccion de retorno de la funcion actual.
     * Es el registro MAS IMPORTANTE para context switch:
     * Cuando restauramos pc y hacemos 'ret', saltamos al codigo
     * del nuevo proceso en la posicion exacta donde se pauso.
     */
    unsigned long pc;
    
    /**
     * @brief sp (Stack Pointer)
     * 
     * Apunta al tope del stack del proceso.
     * Cada proceso tiene su propio stack en process_stack[][].
     * Se restaura para que el nuevo proceso use su stack correcto.
     */
    unsigned long sp;
};

/* ========================================================================== */
/* BLOQUE DE CONTROL DE PROCESO (PCB)                                       */
/* ========================================================================== */

/**
 * @struct pcb
 * @brief Process Control Block - Ficha de identidad de un proceso
 * 
 * @details
 *   Contiene TODA la informacion del proceso:
 *   - Registros guardados (para reanudar)
 *   - Estado actual (RUNNING, READY, BLOCKED, etc)
 *   - Identificadores (PID, prioridad)
 *   - Contadores (preemption count)
 *   
 *   TAMAÑO APROXIMADO: ~120 bytes (cpu_context = 104, otras = 16)
 *   Con MAX_PROCESS = 64, array total = ~7.5 KB
 *   (Pequeno comparado con los 256KB de stacks)
 * 
 * @see cpu_context para explicacion de registros
 * @see kernel.c process[] array que almacena todos los PCBs
 */
struct pcb {
    /**
     * @brief Contexto de CPU (registros guardados)
     * 
     * Estos registros se guardan/restauran durante context switch.
     * El cpu_switch_to() en entry.S lee/escribe este campo.
     * 
     * @see struct cpu_context
     */
    struct cpu_context context;
    
    /**
     * @brief Estado actual del proceso
     * 
     * Valores: PROCESS_RUNNING, PROCESS_READY, PROCESS_BLOCKED, PROCESS_ZOMBIE
     * 
     * El scheduler solo selecciona procesos en estado READY o RUNNING.
     * Los procesos BLOCKED no participan hasta ser desbloqueados.
     * 
     * @see ProcessStates para definiciones
     */
    long state;
    
    /**
     * @brief Process ID (identificador unico)
     * 
     * Valor: 0 a MAX_PROCESS-1
     * Asignado al crear el proceso (create_thread).
     * El proceso 0 es el kernel mismo.
     * 
     * Usado para debugging y para indexar en el array process[].
     */
    long pid;
    
    /**
     * @brief Contador de preemption (puede ser preemptado)
     * 
     * En kernels mas complejos, se usa para:
     *   0 = Puede ser preemptado por interrupciones
     *   >0 = En seccion critica, no puede ser preemptado
     * 
     * En nuestro kernel simplificado, siempre es 0.
     */
    long prempt_count;
    
    /**
     * @brief Prioridad del proceso
     * 
     * Rango: 0-255 (0 = maxima prioridad, 255 = minima)
     * 
     * El scheduler elige el proceso con MENOR prioridad (mas urgente).
     * La prioridad se modifica por "aging" mientras el proceso espera.
     * 
     * ALGORITMO DE SCHEDULER:
     *   1. AGING: Mientras el proceso espera, priority--
     *   2. SELECCION: Elegir el de MENOR priority
     *   3. PENALIZACION: Al seleccionar, priority += 2
     * 
     * @see schedule() en kernel.c para detalles
     */
    int priority;
};

#endif // SCHED_H