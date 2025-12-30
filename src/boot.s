/* src/boot.S */
/* Bootloader en ensamblador para ARM64 */
.section .text /* Punto de entrada para el programa */
.global _start /* Hacemos _start visible al linker */

_start:
    /* 1. Comprobar el ID del procesador (MPIDR_EL1) */
    mrs x0, MPIDR_EL1
    and x0, x0, #3 /* Nos quedamos con los ultimos 2 bits */
    cbz x0, master /* Si es el nucleo 0, salta a master */
    b hang /* El resto de nucleos se quedan en bucle infinito */

master:
    /* 2. Limpiar el registro BSS (donde van las variables globales no inicializadas) 
        (Por simplicidad,  en este paso lo omitimos, pero es una buana practica) */
    
    /* 3. Configuramos el Stack Pointer (SP) */
    /* El stack crece hacia abajo, asi que lo ponemos al final de una zona segura */
    ldr x0, =_stack_top
    mov sp, x0

    /* 4. Saltar a codigo C */
    bl kernel

    /* 5. Si kernel_main retorna, detener el sistema */
hang:
    wfe /* Wait For Event - el resto de nucleos se quedan en bucle infinito */
    b hang
