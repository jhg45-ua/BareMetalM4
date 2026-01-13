ENTRY(_start)

PHDRS {
  text PT_LOAD FLAGS(5); /* R X */
  data PT_LOAD FLAGS(6); /* R W */
}

SECTIONS {
  . = 0x40000000;

  .text : {
    *(.text*)
    *(.rodata*)   /* mete rodata en RX */
  } :text

  . = ALIGN(0x1000);
  .data : { *(.data*) } :data

  .bss (NOLOAD) : {
    *(.bss*)
    *(COMMON)
  } :data

  . = ALIGN(0x1000);
  /* Reserva de stack */
  .stack (NOLOAD) : {
    . = . + 0x1000; /* 4KB */
    _stack_top = .;
  } :data

  . = ALIGN(4096);  /* Alineamos a 4KB por seguridad de la MMU */
  _end = .;         /* Este símbolo marca dónde empieza la memoria libre */
}