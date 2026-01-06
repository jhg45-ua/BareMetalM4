/* 
 * src/locks.S - Primitivas de Sincronizacion Atomicas
 * 
 * Implementa spinlocks usando instrucciones exclusivas ARM64 (LDXR/STXR)
 * para resolver race conditions en sistemas multitarea.
 */

.global spin_lock
.global spin_unlock

/* 
 * spin_lock - Adquiere un spinlock de forma atomica
 * 
 * Parametro: x0 = direccion del lock (0=libre, 1=tomado)
 * 
 * Algoritmo:
 *   1. Leer lock (LDXR)
 *   2. Si tomado, volver a 1 (spin)
 *   3. Si libre, intentar escribir 1 (STXR)
 *   4. Si falla, volver a 1 (alguien se adelanto)
 *   5. Si exito, tenemos el lock
 */
spin_lock:
    mov w1, #1
    
1:  /* Bucle de espera activa */
    ldxr w2, [x0]      /* Lee lock + marca como exclusiva */
    cbnz w2, 1b        /* Si tomado (!=0), reintentar */
    stxr w3, w1, [x0]  /* Intentar escribir 1 */
    cbnz w3, 1b        /* Si fallo (!=0), reintentar */
    dmb sy             /* Barrera de memoria (sincronizar) */
    ret

/* 
 * spin_unlock - Libera un spinlock
 * 
 * Parametro: x0 = direccion del lock
 * 
 * Garantiza que todas las operaciones de la seccion critica
 * se completen antes de liberar el lock.
 */
spin_unlock:
    dmb sy             /* Completar operaciones previas */
    stlr wzr, [x0]     /* Escribir 0 (libre) con release semantics */
    ret