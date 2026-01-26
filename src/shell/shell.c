/**
 * @file shell.c
 * @brief Shell interactivo y procesos de prueba
 * 
 * @details
 *   Implementa:
 *   - Shell interactivo con comandos del sistema
 *   - Procesos de prueba para demostrar multitarea
 *   - Comandos de test para las nuevas características:
 *     * test_rr: Round-Robin con quantum
 *     * test_sem: Semáforos con wait queues
 *     * test_page_fault: Demand paging
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.5
 */

#include "../../include/sched.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/utils/kutils.h"
#include "../../include/shell/shell.h"
#include "../../include/utils/tests.h"
#include "../../include/fs/vfs.h"

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
 * @details
 *   Shell con línea de comandos que soporta:
 *   - help: Muestra comandos disponibles
 *   - ps: Lista de procesos activos (con estados SLEEP/WAIT)
 *   - test_rr: Test de Round-Robin con quantum
 *   - test_sem: Test de semáforos con wait queues
 *   - test_page_fault: Test de demand paging
 *   - clear: Limpia la pantalla
 *   - panic: Provoca un kernel panic
 *   - poweroff: Apaga el sistema
 *   
 *   El shell usa sleep(1) para ceder CPU eficientemente mientras
 *   espera entrada del usuario (NO busy-wait).
 */
void shell_task(void) {
    enable_interrupts();
    
    char command_buf[64];
    int idx = 0;

    kprintf("\n[SHELL] Bienvenido a BareMetalM4 OS v0.5\n");
    kprintf("[SHELL] Escribe 'help' para ver comandos.\n");
    kprintf("> "); // Prompt

    while (1) {
        /* 1. Intentamos leer una tecla */
        const char c = uart_getc_nonblocking();

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

            /* --- PARSER DEL COMANDO (Dividir en Comando y Argumento) --- */
            char cmd[16] = {0};
            char arg[32] = {0};
            int i = 0, j = 0;

            /* Leer la primera palabra (el comando) */
            while (command_buf[i] != ' ' && command_buf[i] != '\0' && i < 15) {
                cmd[i] = command_buf[i];
                i++;
            }
            cmd[i] = '\0';

            /* Si hay un espacio, lo que sigue es el argumento */
            if (command_buf[i] == ' ') {
                i++; /* Saltar el espacio */
                while (command_buf[i] != '\0' && j < 31) {
                    arg[j++] = command_buf[i++];
                }
            }
            arg[j] = '\0';

            /* --- EJECUCIÓN DE COMANDOS --- */
            if (k_strcmp(command_buf, "help") == 0) {
                kprintf("Comandos disponibles:\n");
                kprintf("  help               - Muestra esta ayuda\n");
                kprintf("  ps                 - Lista los procesos (simulado)\n");
                kprintf("  touch [archivo]    - Crea un archivo vacio\n");
                kprintf("  rm [archivo]       - Borra un archivo\n");
                kprintf("  ls                 - Lista los archivos\n");
                kprintf("  cat [archivo]      - Lee el contenido de un archivo\n");
                kprintf("  write [archivo]    - Escribe texto en un archivo\n");
                kprintf("  test [modulo]      - Ejecuta tests. Modulos: all, rr, sem, pf\n");
                kprintf("  clear              - Limpia la pantalla\n");
                kprintf("  panic              - Provoca un Kernel Panic\n");
                kprintf("  poweroff           - Apaga el sistema\n");
            } 
            else if (k_strcmp(command_buf, "ps") == 0) {
                kprintf("\nPID   | Prio   |  State  |   Time   | Name\n");
                kprintf("------|--------|---------|----------|------\n");
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

                    kprintf(" %d    |  %d    | %s    | %d      | %s\n",
                            process[i].pid,
                            process[i].priority,
                            estado_str,
                            process[i].cpu_time,
                            process[i].name);
                }
                kprintf("\n");
            }
            else if (k_strcmp(cmd, "ls") == 0) {
                vfs_ls();
            }
            else if (k_strcmp(cmd, "touch") == 0) {
                if (arg[0] == '\0') kprintf("Uso: touch [nombre_archivo]\n");
                else vfs_create(arg);
            }
            else if (k_strcmp(cmd, "rm") == 0) {
                if (arg[0] == '\0') kprintf("Uso: rm [nombre_archivo]\n");
                else vfs_remove(arg);
            }
            else if (k_strcmp(cmd, "cat") == 0) {
                if (arg[0] == '\0') kprintf("Uso: cat [nombre_archivo]\n");
                else {
                    int fd = vfs_open(arg);
                    if (fd >= 0) {
                        char read_buf[128];
                        int bytes = vfs_read(fd, read_buf, 127);
                        read_buf[bytes] = '\0';
                        kprintf("\n%s\n", read_buf);
                        vfs_close(fd); /* <-- IMPORTANTE: Cerramos el archivo */
                    }
                }
            }
            else if (k_strcmp(cmd, "write") == 0) {
                if (arg[0] == '\0') kprintf("Uso: write [nombre_archivo]\n");
                else {
                    const int fd = vfs_open(arg);
                    if (fd >= 0) {
                        const char *msg = "Texto generado dinamicamente desde la Shell.\n";
                        int len = k_strlen(msg);
                        vfs_write(fd, msg, len);
                        kprintf("Escritos %d bytes en '%s'.\n", len, arg);
                        vfs_close(fd); /* <-- IMPORTANTE */
                    }
                }
            }
            else if (k_strcmp(cmd, "test") == 0) {
                /* Si no hay argumento o es "all", ejecutamos la batería completa original */
                if (arg[0] == '\0' || k_strcmp(arg, "all") == 0) {
                    kprintf("Iniciando bateria de tests general...\n");
                    test_memory();

                    // test_processes();

                    test_scheduler();
                }
                /* Test del Tema 2: Round-Robin y Quantum */
                else if (k_strcmp(arg, "rr") == 0) {
                    test_quantum();
                }
                /* Test del Tema 3: Semáforos y Wait Queues */
                else if (k_strcmp(arg, "sem") == 0) {
                    test_semaphores_efficiency();
                }
                /* Test del Tema 4: Memoria Virtual y Page Faults */
                else if (k_strcmp(arg, "pf") == 0) {
                    create_process((void(*)(void*)) test_demand, nullptr, 0, "test_page_fault");
                }
                /* Argumento no reconocido */
                else {
                    kprintf("Error: Modulo de test '%s' no existe.\n", arg);
                    kprintf("Opciones validas: all, rr, sem, pf\n");
                }
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
