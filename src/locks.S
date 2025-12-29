/* src/locks.S */
.global spin_lock
.global spin_unlock

/* void spin_lock(volatile int *lock_addr) 
    x0 = contiene la direccion de la variable cerrojo */
spin_lock:
    mov w1, #1      /* El valor '1' significa CERRADO */
1: 
    ldxr w2, [x0]   /* (Load Exclusive) Leemos el valor actual del cerrojo */
    cbnz w2, 1b     /* Si no es 0 (esta cerrado), volvemos a intentar (spin) */

    stxr w3, w1, [x0]   /* (Store Exclusive) Intentamos escribir un 1 */
    cbnz w3, 1b         /* Si w3 no es 0, la escritura fallo (alguien se colo), reintentar */

    dmb sy              /* Data Memory Barrier: Asegura que todo lo anterior se guarde antes de seguir */
    ret

/* void spin_unlock(volatile int *lock_addr) */
spin_unlock:
    dmb sy              /* Asegurar que todas las operaciones criticas acabaron antes de soltar */
    stlr wzr, [x0]      /* Escribimos 0 (ABIERTO) usando Store-Release */
    ret