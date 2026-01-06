/**
 * @file scheduler.h
 * @brief Planificador de procesos
 * 
 * @details
 *   Algoritmo de scheduling con prioridades y aging:
 *   - Envejecimiento de procesos en espera
 *   - Selección del proceso con mayor prioridad (menor valor)
 *   - Penalización para evitar monopolio
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

/* Contador global de ticks del sistema */
extern volatile unsigned long sys_timer_count;

/**
 * @brief Planificador de procesos (Scheduler)
 * 
 * Implementa algoritmo de prioridades con aging:
 * 1. Envejecimiento: Disminuye priority de procesos READY
 * 2. Selección: Elige proceso con menor priority
 * 3. Penalización: Aumenta priority del seleccionado
 */
void schedule(void);

/**
 * @brief Manejador de ticks del timer
 * 
 * Se ejecuta en cada interrupción del timer:
 * - Incrementa contador de ticks
 * - Despierta procesos bloqueados cuando corresponde
 */
void timer_tick(void);

/**
 * @brief Duerme el proceso actual durante un número de ticks
 * @param ticks Número de ticks del timer a dormir
 */
void sleep(unsigned int ticks);

#endif /* SCHEDULER_H */
