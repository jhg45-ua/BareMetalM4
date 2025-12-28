ENTRY(_start)

SECTIONS
{
    /* Empezamos en la direccion de RAM de QEMU */
    . = 0x40000000;

    .text : { *(.text) }     /* Codigo */
    .data : { *(.data) }     /* Datos inicializados */
    .bss :  { *(.bss) }      /* Datos sin inicializar */

    /* Reservamos espacio para el Stack */
    . = . + 0x1000;     /* 4KB de Stack */
    _stack_top = .;
}