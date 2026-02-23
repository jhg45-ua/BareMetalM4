/**
 * @file process.h
 * @brief Gestión de procesos del kernel
 * 
 * @details
 *   Funciones y estructuras para la gestión de procesos:
 *   - Creación de procesos del kernel
 *   - Terminación de procesos
 *   - Acceso a las estructuras de procesos (PCB)
 *   - PCBs con soporte para Round-Robin (quantum) y Wait Queues (next)
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "../sched.h"

/* Variables globales de procesos */
extern struct pcb process[MAX_PROCESS];
extern struct pcb *current_process;
extern int num_process;

/**
 * @brief Crea un nuevo thread del kernel
 * @param fn Función a ejecutar
 * @param arg Argumento para la función
 * @param priority Prioridad inicial del proceso
 * @param name Nombre descriptivo del proceso (máx 15 chars)
 * @return PID del proceso creado, -1 en caso de error
 * 
 * @details
 *   Crea un proceso READY que será planificado por el Round-Robin scheduler.
 *   El quantum se asignará cuando el proceso sea elegido por primera vez.
 */
long create_process(void (*fn)(void*), void *arg, int priority, const char *name);

/**
 * @brief Inicializa el subsistema de gestión de procesos
 * 
 * @details
 *   Configura el proceso 0 (IDLE) que se ejecuta cuando no hay
 *   otros procesos READY. Inicializa estructuras globales (PCBs).
 */
void init_process_system();

/**
 * @brief Termina el proceso actual y cede la CPU
 * 
 * @details
 *   Marca el proceso como ZOMBIE. El reaper (free_zombie) lo limpiará luego.
 */
void exit(void);

/**
 * @brief Hook post-fork (reservado para futuras extensiones)
 * 
 * @details
 *   Placeholder para lógica post-context-switch.
 *   Puede ser usado en el futuro para estadísticas o notificaciones.
 */
void schedule_tail(void);

/**
 * @brief Libera recursos de procesos ZOMBIE (reaper)
 * 
 * @details
 *   Recorre process[] y libera stacks de procesos ZOMBIE,
 *   marcándolos como UNUSED para reutilización.
 */
void free_zombie();

#endif /* PROCESS_H */
