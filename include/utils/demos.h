/**
 * @file demos.h
 * @brief Codigo educativo de referencia - No invocado en la demo principal
 * 
 * @details
 *   Declaraciones de funciones educativas que demuestran conceptos
 *   avanzados del sistema operativo (modo usuario, syscalls, proteccion
 *   de memoria) pero que NO se invocan en la ejecucion normal del kernel.
 *   
 *   Se mantienen como referencia para los temas:
 *   - Tema 1: Syscalls (SVC desde EL0)
 *   - Tema 4: Proteccion de memoria (Data Abort en NULL)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.6
 */

#ifndef DEMOS_H
#define DEMOS_H

/**
 * @brief Proceso de usuario en EL0 que ejecuta syscalls
 * @details Referencia educativa - Tema 1 (Syscalls)
 */
void user_task(void);

/**
 * @brief Proceso que intenta violar segmentacion de memoria
 * @details Referencia educativa - Tema 4 (Proteccion de memoria)
 */
void kamikaze_test(void);

/**
 * @brief Funcion wrapper para transicion EL1 -> EL0
 * @param arg Puntero a user_context con pc y sp del usuario
 * @details Referencia educativa - Tema 1 (Modo usuario)
 */
void kernel_to_user_wrapper(void *arg);

/**
 * @brief Crea un proceso de usuario (EL0)
 * @param user_fn Funcion que se ejecutara en modo usuario
 * @param name Nombre descriptivo del proceso
 * @return PID del proceso creado, -1 en caso de error
 * @details Referencia educativa - Tema 1/4 (Modo usuario + proteccion)
 */
long create_user_process(void (*user_fn)(void), const char *name);

#endif /* DEMOS_H */
