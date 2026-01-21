/**
 * @file tests.h
 * @brief Funciones de prueba del sistema
 * 
 * @details
 *   Define pruebas para verificar el funcionamiento
 *   del sistema de memoria y procesos.
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

void user_task();

void kamikaze_test();

void test_quantum(void);

void test_semaphores_efficiency(void);


#endif //TESTS_H