/* src/kernel.c */
// #include <stdint.h>

/* Direccion fisica del UART0 en QEMU 'virt' machine */
volatile unsigned int * const UART0_DIR = (unsigned int *)0x09000000;

/* Funcion auxiliar para escribir un caracter  */
void uart_putc(char c) {
    *UART0_DIR = c; /* Escribir en esta direccion envia el dato a la terminal */
}

/* Funcion auxiliar para escribir una cadena */
void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        s++;
    }
}

/* Punto de entrada principal */
void kernel() {
    uart_puts("¡¡¡Hola desde BareMetalM4!!!\n");
    uart_puts("Sistema Operativo iniciando...\n");

    /* Bucle infinito para que el SO no termine */
    while(1) {}
}