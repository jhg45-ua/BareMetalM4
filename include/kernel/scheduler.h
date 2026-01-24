/**
 * @file scheduler.h
 * @brief Planificador Round-Robin con Quantum y Prioridades
 * 
 * @details
 *   Interfaz del scheduler híbrido del sistema operativo:
 *   
 *   CARACTERÍSTICAS IMPLEMENTADAS:
 *   - Round-Robin con Quantum (preemptive multitasking)
 *   - Prioridades con aging (fairness)
 *   - Sleep eficiente sin busy-wait (wait queues)
 *   
 *   ALGORITMO:
 *   1. Aging: Procesos READY envejecen (priority--)
 *   2. Selección: Se elige el proceso con menor priority
 *   3. Penalización: El seleccionado aumenta su priority
 *   4. Quantum: Cada proceso recibe DEFAULT_QUANTUM ticks
 *   5. Expropriación: Cuando quantum=0, se marca need_reschedule
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

/* Contador global de ticks del sistema */
extern volatile unsigned long sys_timer_count;

/**
 * @brief Planificador de procesos (Scheduler)
 * 
 * @details
 *   Implementa un algoritmo híbrido de scheduling:
 *   
 *   FASES:
 *   1. Aging: Disminuye priority de procesos READY
 *   2. Selección: Elige proceso con menor priority
 *   3. Penalización: Aumenta priority del seleccionado
 *   4. Quantum: Asigna DEFAULT_QUANTUM al nuevo proceso
 *   
 *   Ignora procesos BLOCKED (durmiendo o esperando semáforos).
 */
void schedule(void);

/**
 * @brief Manejador de ticks del timer
 * 
 * @details
 *   Ejecutado en cada interrupción del timer (IRQ):
 *   
 *   FUNCIONES:
 *   1. Incrementa sys_timer_count (reloj global)
 *   2. Decrementa quantum del proceso actual
 *   3. Marca need_reschedule cuando quantum=0 (expropriación)
 *   4. Despierta procesos con wake_up_time <= sys_timer_count
 *   
 *   Implementa Round-Robin con Quantum (preemptive multitasking).
 */
void timer_tick(void);

/**
 * @brief Duerme el proceso actual durante un número de ticks
 * @param ticks Número de ticks del timer a dormir
 * 
 * @details
 *   Implementa sleep eficiente:
 *   - Marca proceso como BLOCKED (block_reason = SLEEP)
 *   - NO consume CPU mientras duerme (NO busy-wait)
 *   - timer_tick() lo despertará automáticamente
 */
void sleep(unsigned int ticks);

#endif /* SCHEDULER_H */
