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
 * @version 0.3
 */

#include "../../include/types.h"
#include "../../include/sched.h"
#include "../../include/drivers/io.h"
#include "../../include/kernel/process.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/shell.h"

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
 * - ps: Lista procesos activos
 * - clear: Limpia la pantalla
 * - panic: Provoca un kernel panic
 * - poweroff: Apaga el sistema
 */
void shell_task(void) {
    enable_interrupts();
    
    char command_buf[64];
    int idx = 0;

    kprintf("\n[SHELL] Bienvenido a BareMetalM4 OS v0.3\n");
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
                kprintf("  help   - Muestra esta ayuda\n");
                kprintf("  ps     - Lista los procesos (simulado)\n");
                kprintf("  clear  - Limpia la pantalla\n");
                kprintf("  panic  - Provoca un Kernel Panic\n");
                kprintf("  poweroff - Apaga el sistema\n");
            } 
            else if (k_strcmp(command_buf, "ps") == 0) {
                kprintf("\nPID | Priority  | State | Name\n");
                kprintf("----|-----------|-------|------\n");
                for(int i=0; i<num_process; i++) {
                     /* OJO: Si el proceso es ZOMBIE, lo indicamos */
                     const char *estado = (process[i].state == 0) ? "RUN " : 
                                          (process[i].state == 1) ? "RDY " : 
                                          (process[i].state == 2) ? "BLK " : "ZOMB";
                                          
                     kprintf(" %d  |    %d     | %s  | %s\n", 
                            process[i].pid, 
                            process[i].priority, 
                            estado,
                            process[i].name);
                }
                kprintf("\n");
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

/* ========================================================================== */
/* PROCESOS DE USUARIO (PROCESOS DE PRUEBA)                                 */
/* ========================================================================== */

/**
 * @brief Primer proceso de usuario (multitarea expropiativa)
 */
void proceso_1(void) {
    enable_interrupts();
    int c = 0;
    while(1) {
        kprintf("[P1] Proceso 1 (Cuenta: %d)\n", c++);
        sleep(70); 
    }
}

/**
 * @brief Segundo proceso de usuario (multitarea expropiativa)
 */
void proceso_2(void) {
    enable_interrupts();
    int c = 0;
    while(1) {
        kprintf("     [P2] Proceso 2 (Cuenta: %d)\n", c++);
        sleep(10);
    }
}

/**
 * @brief Tarea que cuenta hasta 3 y muere
 * 
 * Este proceso demuestra la terminación de procesos.
 * Al finalizar, ret_from_fork captura el retorno y llama a exit().
 */
void proceso_mortal(void) {
    enable_interrupts();
    
    for (int i = 0; i < 3; i++) {
        // kprintf("     [MORTAL] Vida restante: %d\n", 3 - i);
        sleep(15); /* Duerme un poco */
    }

    // kprintf("     [MORTAL] Adios mundo cruel...\n");
    /* AQUÍ OCURRE LA MAGIA:
       Al terminar el for, la función hace 'return'.
       El wrapper 'ret_from_fork' captura ese retorno y llama a 'exit()'. */
}
