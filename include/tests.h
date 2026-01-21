/**
 * @file tests.h
 * @brief Funciones de prueba del sistema
 * 
 * @details
 *   Define pruebas para verificar el funcionamiento
 *   del sistema de memoria, procesos y sincronización.
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef TESTS_H
#define TESTS_H

/**
 * @brief Ejecuta pruebas del sistema de memoria
 * 
 * Verifica el funcionamiento de kmalloc/kfree y la MMU.
 */
void test_memory();

/**
 * @brief Lanza pruebas de creación y destrucción
 */
void test_processes(void);

/**
 * @brief Lanza pruebas de scheduler y sleep
 */
void test_scheduler(void);

/**
 * @brief Proceso de usuario en EL0 que ejecuta syscalls
 */
void user_task();

/**
 * @brief Proceso que intenta violar segmentación de memoria
 */
void kamikaze_test();

/**
 * @brief Prueba Round-Robin con proceso egoísta
 */
void test_quantum(void);

/**
 * @brief Prueba wait queues en semáforos
 */
void test_semaphores_efficiency(void);


#endif //TESTS_H