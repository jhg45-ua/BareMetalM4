/**
 * @file process.h
 * @brief Gestión de procesos del kernel
 * 
 * @details
 *   Funciones y estructuras para la gestión de procesos:
 *   - Creación de threads
 *   - Terminación de procesos
 *   - Acceso a las estructuras de procesos
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "../sched.h"

/* Variables globales de procesos */
extern struct pcb process[MAX_PROCESS];
extern struct pcb *current_process;
extern int num_process;
// extern uint8_t process_stack[MAX_PROCESS][4096];

/**
 * @brief Crea un nuevo thread del kernel
 * @param fn Función a ejecutar
 * @param priority Prioridad inicial del proceso
 * @param name Nombre descriptivo del proceso (máx 15 chars)
 * @return PID del proceso creado, -1 en caso de error
 */
long create_process(void (*fn)(void), int priority, const char *name);

/**
 * @brief Crea un Hilo del Kernel (Kernel Thread)
 * * En BareMetalM4 (v0.3), como no hay separación de memoria virtual por proceso,
 * todos los procesos son técnicamente hilos del kernel que comparten espacio de direcciones.
 */
long create_thread(void (*fn)(void), int priority, const char *name);

void init_process_system();

/**
 * @brief Termina el proceso actual y cede la CPU
 */
void exit(void);

/**
 * @brief Hook post-fork (reservado para futuras extensiones)
 */
void schedule_tail(void);

void free_zombie();

#endif /* PROCESS_H */
