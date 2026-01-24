/**
 * @file tests.h
 * @brief Suite de pruebas del sistema operativo
 * 
 * @details
 *   Define pruebas para verificar las funcionalidades implementadas:
 *   
 *   SCHEDULER:
 *   - Round-Robin con Quantum (expropriación por tiempo)
 *   - Prioridades con aging (evita starvation)
 *   - Sleep y wake-up eficientes (wait queues)
 *   
 *   SINCRONIZACIÓN:
 *   - Semáforos con Wait Queues (NO busy-wait)
 *   - Bloqueo eficiente sin consumo de CPU
 *   - Despertar automático al liberar recursos
 *   
 *   MEMORIA:
 *   - Heap dinámico (kmalloc/kfree, First-Fit)
 *   - Paginación por demanda (Demand Paging)
 *   - Asignación automática de páginas en Page Fault
 *   
 *   SEGURIDAD Y SYSCALLS:
 *   - Separación de privilegios (EL0/EL1)
 *   - Manejo robusto de excepciones
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef TESTS_H
#define TESTS_H

/* ========================================================================== */
/* PRUEBAS DE HEAP DINÁMICO                                                 */
/* ========================================================================== */

/**
 * @brief Ejecuta pruebas del sistema de memoria dinámica
 * 
 * @details
 *   Verifica el funcionamiento del gestor de heap:
 *   - Estado de la MMU consultando SCTLR_EL1 (bit M = MMU habilitada)
 *   - Asignación dinámica con kmalloc (algoritmo First-Fit)
 *   - Escritura y lectura en memoria heap
 *   - Liberación de memoria con kfree
 *   
 *   Nota: Para probar paginación por demanda, usar test_demand()
 */
void test_memory(void);

/* ========================================================================== */
/* PRUEBAS DE PROCESOS                                                       */
/* ========================================================================== */

/**
 * @brief Lanza pruebas de creación y destrucción de procesos
 * 
 * @details
 *   Crea 3 procesos mortales que terminan automáticamente.
 *   Valida el ciclo de vida completo:
 *   - UNUSED -> READY -> RUNNING -> SLEEPING -> ZOMBIE -> UNUSED
 *   - Limpieza automática de zombies por free_zombie()
 *   - Liberación de memoria del stack (kmalloc/kfree)
 */
void test_processes(void);

/**
 * @brief Lanza pruebas de scheduler Round-Robin con sleep
 * 
 * @details
 *   Crea dos procesos con diferentes tiempos de sleep:
 *   - Proceso Lento: Duerme 70 ticks entre iteraciones
 *   - Proceso Rápido: Duerme 10 ticks entre iteraciones
 *   
 *   Valida:
 *   - Gestión eficiente de procesos SLEEPING (no consumen CPU)
 *   - Despertar automático en timer_tick() cuando se cumple wake_up_time
 *   - Algoritmo de prioridades con aging para evitar starvation
 */
void test_scheduler(void);

/* ========================================================================== */
/* PRUEBAS DE SYSCALLS Y SEGURIDAD                                           */
/* ========================================================================== */

/**
 * @brief Proceso de usuario en EL0 que ejecuta syscalls
 * 
 * @details
 *   Demuestra el uso de llamadas al sistema desde nivel de usuario:
 *   - SYS_WRITE (0): Imprime mensaje en consola
 *   - SYS_EXIT (1): Termina el proceso limpiamente
 *   
 *   Utiliza ensamblador inline para invocar syscalls via SVC.
 */
void user_task(void);

/**
 * @brief Proceso que intenta violar segmentación de memoria
 * 
 * @details
 *   Prueba de robustez del manejo de excepciones.
 *   Intenta escribir en dirección NULL (0x0) para generar un Data Abort.
 *   El sistema debe capturar la excepción y terminar el proceso sin colapsar.
 */
void kamikaze_test(void);

/* ========================================================================== */
/* PRUEBAS DE ROUND-ROBIN CON QUANTUM                                       */
/* ========================================================================== */

/**
 * @brief Prueba de Round-Robin con Quantum
 * 
 * @details
 *   Crea un proceso egoísta que intenta monopolizar la CPU con un bucle infinito.
 *   
 *   FUNCIONAMIENTO:
 *   - Cada proceso recibe un quantum (ej. 10 ticks)
 *   - En cada timer_tick(), el quantum se decrementa
 *   - Cuando quantum llega a 0, se marca need_reschedule
 *   - El scheduler expropria el proceso y asigna la CPU a otro
 *   
 *   RESULTADO ESPERADO:
 *   - El proceso egoísta NO monopoliza el sistema
 *   - El shell y otros procesos siguen funcionando
 *   - Demuestra que el Round-Robin con Quantum funciona correctamente
 */
void test_quantum(void);

/* ========================================================================== */
/* PRUEBAS DE SEMÁFOROS CON WAIT QUEUES                                     */
/* ========================================================================== */

/**
 * @brief Prueba de semáforos con Wait Queues (NO busy-wait)
 * 
 * @details
 *   Crea dos procesos que compiten por el mismo semáforo:
 *   - Holder: Toma el semáforo y lo mantiene ocupado 5 segundos
 *   - Waiter: Intenta tomarlo y debe bloquearse eficientemente
 *   
 *   COMPORTAMIENTO CON WAIT QUEUES:
 *   - Waiter se marca como BLOCKED (block_reason = BLOCK_REASON_SEM)
 *   - Se añade a la wait queue del semáforo
 *   - NO consume CPU mientras espera (NO busy-wait)
 *   - Es despertado automáticamente cuando Holder libera el semáforo
 *   
 *   VALIDACIÓN:
 *   - Sincronización correcta sin busy-wait
 *   - Eficiencia: Los procesos bloqueados no aparecen en schedule()
 *   - Mejora significativa vs. implementaciones antiguas con busy-wait
 */
void test_semaphores_efficiency(void);

/* ========================================================================== */
/* PRUEBAS DE PAGINACIÓN POR DEMANDA                                        */
/* ========================================================================== */

/**
 * @brief Prueba de paginación por demanda (Demand Paging)
 * 
 * @details
 *   Intenta escribir en memoria no mapeada (0x50000000).
 *   
 *   CON DEMAND PAGING IMPLEMENTADO:
 *   1. Se produce un Page Fault (Data Abort) controlado
 *   2. El handler de excepciones detecta que es un fallo de página
 *   3. El kernel asigna la página físicamente bajo demanda
 *   4. Actualiza las tablas de páginas (PTE)
 *   5. La instrucción se reintenta y la escritura se completa con éxito
 *   
 *   SIN DEMAND PAGING:
 *   - El proceso sería terminado por handle_fault()
 *   
 *   Esta técnica permite asignar memoria solo cuando realmente se usa,
 *   optimizando el uso de RAM (lazy allocation).
 */
void test_demand(void);

#endif /* TESTS_H */