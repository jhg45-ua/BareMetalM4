/**
 * @file locks.S
 * @brief Primitivas de sincronización atómicas (Assembly ARM64)
 * 
 * @details
 *   Implementa spinlocks usando instrucciones exclusivas ARM64 (LDXR/STXR)
 *   para resolver race conditions en sistemas multitarea.
 * 
 *   Instrucciones exclusivas:
 *   - LDXR: Load Exclusive Register (marca dirección como exclusiva)
 *   - STXR: Store Exclusive Register (escribe solo si sigue exclusiva)
 *   - DMB: Data Memory Barrier (sincronización de memoria)
 * 
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.3
 */

.global spin_lock
.global spin_unlock

/* 
 * spin_lock - Adquiere un spinlock de forma atómica
 * 
 * Parámetros:
 *   x0 = Dirección del lock (0=libre, 1=tomado)
 * 
 * Algoritmo (Test-and-Set atómico):
 *   1. Leer lock (LDXR) marcando dirección como exclusiva
 *   2. Si tomado (!=0), volver a 1 (spin/busy-wait)
 *   3. Si libre (==0), intentar escribir 1 (STXR)
 *   4. Si STXR falla (!=0), volver a 1 (otro CPU se adelantó)
 *   5. Si STXR exitoso (==0), lock adquirido
 *   6. Barrera DMB garantiza orden de memoria
 * 
 * Este es un spinlock con espera activa (consume CPU).
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
 * Parámetros:
 *   x0 = Dirección del lock
 * 
 * DMB (Data Memory Barrier) garantiza que todas las operaciones
 * de la sección crítica se completen antes de liberar el lock.
 * STLR (Store-Release) escribe 0 con semántica de "release".
 */
spin_unlock:
    dmb sy             /* Completar operaciones previas */
    stlr wzr, [x0]     /* Escribir 0 (libre) con release semantics */
    ret