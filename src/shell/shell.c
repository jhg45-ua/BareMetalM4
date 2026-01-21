/**
 * @file shell.c
 * @brief Shell interactivo y procesos de prueba
 * 
 * @details
 *   Implementa:
 *   - Shell interactivo con comandos del sistema
 *   - Procesos de prueba para demostrar multitarea
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#include "../../include/sched.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/shell.h"
#include "../../include/tests.h"

/* ========================================================================== */
/* FUNCIONES EXTERNAS                                                        */
/* ========================================================================== */

/* Habilita las interrupciones IRQ en el procesador */
extern void enable_interrupts(void);

/* Apaga el sistema */
extern void system_off(void);

/* ========================================================================== */
/* SHELL INTERACTIVO                                                         */
/* ========================================================================== */

/**
 * @brief Tarea del shell interactivo
 * 
 * Shell con línea de comandos que soporta:
 * - help: Muestra comandos disponibles
 * - ps: Lista de procesos activos
 * - clear: Limpia la pantalla
 * - panic: Provoca un kernel panic
 * - poweroff: Apaga el sistema
 */
void shell_task(void) {
    enable_interrupts();
    
    char command_buf[64];
    int idx = 0;

    kprintf("\n[SHELL] Bienvenido a BareMetalM4 OS v0.4\n");
    kprintf("[SHELL] Escribe 'help' para ver comandos.\n");
    kprintf("> "); // Prompt

    while (1) {
        /* 1. Intentamos leer una tecla */
        char c = uart_getc_nonblocking();

        if (c == 0) {
            /* Si no hay tecla, dormimos un poco para no quemar CPU */
            /* Esto hace que el shell sea eficiente */
            sleep(1); 
            continue;
        }

        /* 2. Eco local (mostrar lo que escribes) */
        /* Si es Enter (Carriage Return '\r'), procesamos */
        if (c == '\r') {
            uart_putc('\n'); // Salto de línea visual
            command_buf[idx] = '\0'; // Terminador de string

            /* --- EJECUCIÓN DE COMANDOS --- */
            if (k_strcmp(command_buf, "help") == 0) {
                kprintf("Comandos disponibles:\n");
                kprintf("  help               - Muestra esta ayuda\n");
                kprintf("  ps                 - Lista los procesos (simulado)\n");
                kprintf("  test               - Ejecutando test de memoria, procesos y scheduler\n");
                kprintf("  test_user_mode     - Ejecuta test del modo usuario\n");
                kprintf("  test_crash         - Ejecuta test de proteccion de memoria basica\n");
                kprintf("  clear              - Limpia la pantalla\n");
                kprintf("  panic              - Provoca un Kernel Panic\n");
                kprintf("  poweroff           - Apaga el sistema\n");
            } 
            else if (k_strcmp(command_buf, "ps") == 0) {
                kprintf("\nPID | Prio | State | Time | Name\n");
                kprintf("----|------|-------|------|------\n");
                for(int i = 0; i < MAX_PROCESS; i++) {
                    /* 1. Si el hueco está VACÍO (0), no lo mostramos */
                    if (process[i].state == PROCESS_UNUSED) {
                        continue;
                    }

                    /* 2. Decodificar el estado correctamente */
                    const char *estado_str;
                    switch (process[i].state) {
                        case PROCESS_RUNNING: estado_str = "RUN "; break; /* 1 */
                        case PROCESS_READY:   estado_str = "RDY "; break; /* 2 */
                        case PROCESS_BLOCKED:
                            if (process[i].block_reason == BLOCK_REASON_SLEEP) {
                                estado_str = "SLEEP ";
                                break;
                            } else if (process[i].block_reason == BLOCK_REASON_WAIT) {
                                estado_str = "WAIT ";
                                break;
                            } else {
                                estado_str = "BLK ";
                                break;
                            }
                        case PROCESS_ZOMBIE:  estado_str = "ZOMB"; break; /* 4 */
                        default:              estado_str = "????"; break;
                    }

                    kprintf(" %d  |  %d   | %s  | %d  | %s\n",
                            process[i].pid,
                            process[i].priority,
                            estado_str,
                            process[i].cpu_time,
                            process[i].name);
                }
                kprintf("\n");
            }
            else if (k_strcmp(command_buf, "test") == 0) {
                kprintf("Iniciando bateria de tests..\n");

                test_memory();      /* Test simple sincrono */

                //test_processes();   /* Lanza procesos mortales */

                test_scheduler();   /* Comprueba el scheduler */
            }
            else if (k_strcmp(command_buf, "test_user_mode") == 0) {
                create_process((void(*)(void*)) user_task, 0, 0, "test_user_mode");
            }
            else if (k_strcmp(command_buf, "test_crash") == 0) {
                create_process((void(*)(void*)) kamikaze_test, 0, 0, "test_user_mode");
            }
            else if (k_strcmp(command_buf, "clear") == 0) {
                /* Código ANSI para limpiar terminal */
                kprintf("\033[2J\033[H");
                kprintf("BareMetalM4 Shell\n");
            }
            else if (k_strcmp(command_buf, "panic") == 0) {
                panic("Usuario solicito panico");
            }
            else if(k_strcmp(command_buf, "poweroff") == 0) {
                kprintf("Apagando el sistema... Hasta luego!\n");
                system_off();
            }
            else if (idx > 0) {
                kprintf("Comando desconocido: %s\n", command_buf);
            }

            /* Resetear buffer y prompt */
            idx = 0;
            kprintf("> ");
        } 
        /* Si es Backspace (borrar) */
        else if (c == 127 || c == '\b') {
            if (idx > 0) {
                idx--;
                uart_puts("\b \b"); // Truco visual para borrar en terminal
            }
        }
        /* Carácter normal */
        else {
            if (idx < 63) {
                command_buf[idx++] = c;
                uart_putc(c); // Eco
            }
        }
    }
}
