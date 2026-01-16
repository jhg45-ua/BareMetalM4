/**
 * @file shell.h
 * @brief Shell interactivo del sistema
 * 
 * @details
 *   Shell básico con comandos del sistema
 */

#ifndef SHELL_H
#define SHELL_H

/**
 * @brief Tarea del shell interactivo
 * 
 * Proporciona una interfaz de línea de comandos para:
 * - Listar procesos (ps)
 * - Mostrar ayuda (help)
 * - Limpiar pantalla (clear)
 * - Provocar panic (panic)
 * - Apagar sistema (poweroff)
 */
void shell_task(void);

#endif /* SHELL_H */
