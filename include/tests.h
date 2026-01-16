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
 * @brief Primer proceso de usuario (multitarea expropiativa)
 */
void proceso_1(void);

/**
 * @brief Segundo proceso de usuario (multitarea expropiativa)
 */
void proceso_2(void);

/**
 * @brief Tarea que cuenta hasta 3 y muere
 */
void proceso_mortal(void);

#endif //TESTS_H