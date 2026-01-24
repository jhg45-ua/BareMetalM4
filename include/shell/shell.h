/**
 * @file shell.h
 * @brief Shell interactivo del sistema
 * 
 * @details
 *   Shell básico con comandos del sistema:
 *   - Gestión de procesos (ps)
 *   - Tests de nuevas características (test_rr, test_sem, test_page_fault)
 *   - Utilidades del sistema (help, clear, poweroff)
 */

#ifndef SHELL_H
#define SHELL_H

/**
 * @brief Tarea del shell interactivo
 * 
 * @details
 *   Proporciona una interfaz de línea de comandos para:
 *   - Listar procesos (ps) - Muestra estados SLEEP/WAIT/RUN/RDY
 *   - Ejecutar tests (test_rr, test_sem, test_page_fault)
 *   - Mostrar ayuda (help)
 *   - Limpiar pantalla (clear)
 *   - Provocar panic (panic)
 *   - Apagar sistema (poweroff)
 *   
 *   Usa sleep() para espera eficiente (NO busy-wait).
 */
void shell_task(void);

#endif /* SHELL_H */
