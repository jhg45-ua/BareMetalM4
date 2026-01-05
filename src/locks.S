/* 
 * =============================================================
 * src/locks.S - Primitivas de Sincronizacion Atomicas
 * =============================================================
 * 
 * PROPOSITO:
 *   Implementar spinlocks (cerrojos de espera activa) usando las instrucciones
 *   de exclusividad del hardware ARM64 (LDXR/STXR).
 * 
 * PROBLEMA QUE RESOLVEMOS:
 *   En sistemas multitarea, multiples procesos pueden acceder a recursos
 *   compartidos (buffers, contadores, estructuras de datos). Sin sincronizacion,
 *   obtenemos "race conditions" (condiciones de carrera).
 * 
 *   Ejemplo SIN locks:
 *     Proceso A lee contador=5
 *     Proceso B lee contador=5
 *     Proceso A escribe contador=6
 *     Proceso B escribe contador=6
 *     Resultado: contador=6 (deberia ser 7) ← ¡ERROR!
 * 
 *   Ejemplo CON locks:
 *     Proceso A: spin_lock() → lee=5, escribe=6 → spin_unlock()
 *     Proceso B: spin_lock() (espera...) → lee=6, escribe=7 → spin_unlock()
 *     Resultado: contador=7 ← ¡CORRECTO!
 * 
 * CONCEPTOS CLAVE:
 *   - Atomicidad: Operacion que ocurre "todo o nada", sin interrupciones
 *   - Exclusion mutua: Solo 1 proceso puede estar en la seccion critica
 *   - Spinlock: Lock que "gira" (spin) intentando adquirir el cerrojo
 *   - Memory Barriers: Instrucciones que fuerzan orden de memoria
 * 
 * INSTRUCCIONES ESPECIALES ARM64:
 *   - LDXR: Load Exclusive Register (marca memoria como "monitoreada")
 *   - STXR: Store Exclusive Register (solo escribe si nadie mas accedio)
 *   - DMB:  Data Memory Barrier (ordena accesos a memoria)
 *   - STLR: Store-Release (escribe + barrera de memoria)
 * =============================================================
 */

.global spin_lock
.global spin_unlock

/* 
 * =============================================================
 * FUNCION: spin_lock
 * =============================================================
 * 
 * PROPOSITO:
 *   Adquirir un spinlock de forma atomica. Si el lock ya esta tomado,
 *   "girar" (spin) en un bucle hasta que se libere.
 * 
 * PROTOTIPO C:
 *   void spin_lock(volatile int *lock_addr);
 * 
 * PARAMETRO:
 *   x0 = direccion de memoria del lock (variable entera)
 * 
 * VALOR DEL LOCK:
 *   0 = Lock LIBRE (unlocked)
 *   1 = Lock TOMADO (locked)
 * 
 * ALGORITMO:
 *   1. Leer el valor actual del lock (LDXR)
 *   2. Si no es 0 (esta tomado), volver al paso 1 (spin)
 *   3. Si es 0 (esta libre), intentar escribir 1 (STXR)
 *   4. Si STXR falla (alguien se adelanto), volver al paso 1
 *   5. Si STXR tiene exito, tenemos el lock → salir
 * 
 * POR QUE NO BASTA CON LDR/STR NORMALES:
 *   Ejemplo de fallo con instrucciones normales:
 *   
 *   Tiempo  CPU0                CPU1
 *   -----   ----------------    ----------------
 *   t1      ldr w2, [x0]        ldr w2, [x0]
 *   t2      (w2 = 0, libre!)    (w2 = 0, libre!)
 *   t3      str #1, [x0]        str #1, [x0]
 *   t4      ¡Entra!             ¡Entra tambien!  ← ¡2 PROCESOS EN SECCION CRITICA!
 * 
 *   Con LDXR/STXR:
 *   
 *   Tiempo  CPU0                    CPU1
 *   -----   --------------------    --------------------
 *   t1      ldxr w2, [x0]           ldxr w2, [x0]
 *           (marca exclusiva)       (marca exclusiva, invalida la de CPU0)
 *   t2      stxr w3, w1, [x0]       stxr w3, w1, [x0]
 *           (w3=1, FALLO!)          (w3=0, EXITO!)
 *   t3      Vuelve a intentar...    ¡Entra! ← SOLO UNO ENTRA
 * 
 * HARDWARE EXCLUSIVE MONITOR:
 *   ARM tiene un circuito especial que "monitorea" direcciones de memoria:
 *   - LDXR marca la direccion como "exclusiva" para este core
 *   - Si otro core accede a esa direccion, se invalida la marca
 *   - STXR solo tiene exito si la marca sigue activa
 * =============================================================
 */
spin_lock:
    /*
     * Preparar el valor '1' que significa "lock tomado"
     * 
     * MOV w1, #1
     *   w1 = registro de 32 bits (la 'w' indica 32-bit, 'x' seria 64-bit)
     *   #1 = constante inmediata
     * 
     * ¿POR QUE w1 Y NO x1?
     *   Los locks suelen ser 'int' (32 bits) en C, no 'long' (64 bits).
     *   Usar 'w' es mas eficiente y coincide con el tipo de dato.
     */
    mov w1, #1
    
1:  /* Etiqueta local: bucle de espera activa (spin loop) */
    
    /*
     * LDXR = Load Exclusive Register
     * 
     * LDXR w2, [x0]
     *   Lee el valor de memoria en direccion x0 y lo guarda en w2
     *   ADEMAS: Marca esta direccion como "monitoreada" (exclusive monitor)
     * 
     * EFECTO SECUNDARIO CRITICO:
     *   El hardware ARM ahora "vigila" la direccion x0. Si otro core
     *   (o incluso una interrupcion) accede a x0, la marca se invalida.
     * 
     * Estado despues:
     *   w2 = valor actual del lock (0=libre, 1=tomado)
     *   Exclusive monitor = ACTIVO para direccion x0
     */
    ldxr w2, [x0]
    
    /*
     * CBNZ = Compare and Branch if Not Zero
     * 
     * CBNZ w2, 1b
     *   Si w2 != 0 (lock tomado), saltar a la etiqueta '1' (hacia atras, 'b'=backward)
     *   Si w2 == 0 (lock libre), continuar a la siguiente instruccion
     * 
     * SPIN (ESPERA ACTIVA):
     *   Si el lock esta tomado, este bucle se ejecuta indefinidamente:
     *     ldxr → lee lock → es 1 → cbnz salta → ldxr → lee lock → ...
     *   
     *   Esto consume CPU activamente (por eso "spinlock" = girar).
     *   No es eficiente para esperas largas, pero perfecto para secciones
     *   criticas muy cortas (microsegundos).
     * 
     * ALTERNATIVA: WFE (Wait for Event)
     *   En sistemas reales, se usa 'wfe' en el spin para ahorrar energia:
     *     ldxr w2, [x0]
     *     cbnz w2, wfe_loop
     *     ...
     *   wfe_loop:
     *     wfe          ← Duerme hasta que alguien haga SEV
     *     b 1b
     */
    cbnz w2, 1b

    /*
     * STXR = Store Exclusive Register
     * 
     * STXR w3, w1, [x0]
     *   INTENTA escribir w1 (valor 1) en la direccion x0
     *   Resultado en w3:
     *     0 = Exito (nadie mas accedio a x0 desde el LDXR)
     *     1 = Fallo (alguien invalido el monitor exclusivo)
     * 
     * MECANISMO DE EXCLUSIVIDAD:
     *   STXR solo tiene exito si:
     *   1. Se ejecuto un LDXR previo en este core
     *   2. Nadie mas accedio a la direccion x0 desde entonces
     *   3. No ocurrio un cambio de contexto que invalidara el monitor
     * 
     * CASO DE FALLO:
     *   CPU0 ejecuta LDXR (monitor activo)
     *   CPU1 ejecuta LDXR o escribe en x0 (invalida monitor de CPU0)
     *   CPU0 ejecuta STXR → w3=1 (fallo) → debe reintentar
     * 
     * VENTAJA:
     *   Atomicidad garantizada por hardware. No hay forma de que 2 cores
     *   ejecuten STXR con exito simultaneamente en la misma direccion.
     */
    stxr w3, w1, [x0]
    
    /*
     * Verificar si STXR tuvo exito
     * 
     * CBNZ w3, 1b
     *   Si w3 != 0 (STXR fallo), volver al inicio del bucle
     *   Si w3 == 0 (STXR exitoso), continuar (tenemos el lock!)
     * 
     * ESCENARIO DE FALLO:
     *   Esto ocurre cuando hay "contention" (competencia):
     *   - Multiples cores intentan adquirir el lock simultaneamente
     *   - Una interrupcion ocurre entre LDXR y STXR
     *   - Un cambio de contexto invalida el monitor
     * 
     * En estos casos, simplemente reintentamos el algoritmo completo.
     */
    cbnz w3, 1b

    /*
     * DMB = Data Memory Barrier
     * 
     * DMB sy
     *   sy = "System" barrier (afecta a todo el sistema)
     *   
     * PROPOSITO:
     *   Garantizar que todas las operaciones de memoria ANTERIORES a esta
     *   instruccion se completen ANTES de que continue la ejecucion.
     * 
     * POR QUE ES NECESARIO:
     *   Los procesadores modernos reordenan instrucciones para optimizar:
     *   
     *   Sin DMB (puede ocurrir):
     *     spin_lock(&lock)
     *     buffer[0] = 42;      ← El compilador/CPU podria ejecutar esto ANTES
     *     buffer[1] = 99;         del lock si no hay barrera!
     *   
     *   Con DMB:
     *     spin_lock(&lock)     ← DMB garantiza que el lock se adquiera primero
     *     buffer[0] = 42;      ← Estas escrituras ocurren DESPUES del lock
     *     buffer[1] = 99;
     * 
     * MODELOS DE MEMORIA:
     *   ARM tiene un modelo de memoria "weakly-ordered" (debilmente ordenado).
     *   Esto significa que sin barreras explicitas, el hardware puede:
     *   - Reordenar lecturas y escrituras
     *   - Cachear valores desactualizados
     *   - Ejecutar instrucciones fuera de orden
     *   
     *   DMB fuerza un punto de sincronizacion.
     * 
     * TIPOS DE DMB:
     *   - DMB sy:  System-wide (todos los cores, toda la memoria)
     *   - DMB ish: Inner Shareable (cores del mismo cluster)
     *   - DMB osh: Outer Shareable (clusters diferentes)
     * 
     * COSTO:
     *   DMB es costoso (decenas de ciclos), pero necesario para correccion.
     */
    dmb sy
    
    /*
     * Retornar al codigo que adquirio el lock
     * 
     * En este punto:
     *   - El lock vale 1 (tomado)
     *   - Somos los UNICOS con acceso a la seccion critica
     *   - Todas las operaciones de memoria previas estan sincronizadas
     */
    ret

/* 
 * =============================================================
 * FUNCION: spin_unlock
 * =============================================================
 * 
 * PROPOSITO:
 *   Liberar un spinlock previamente adquirido con spin_lock().
 * 
 * PROTOTIPO C:
 *   void spin_unlock(volatile int *lock_addr);
 * 
 * PARAMETRO:
 *   x0 = direccion de memoria del lock
 * 
 * ALGORITMO:
 *   1. Barrera de memoria (DMB) para asegurar que todas las escrituras
 *      de la seccion critica se completen
 *   2. Escribir 0 (lock libre) usando Store-Release
 * 
 * DIFERENCIA CON spin_lock:
 *   - spin_lock: Complejo (LDXR/STXR loop)
 *   - spin_unlock: Simple (una escritura con barrera)
 * 
 * POR QUE NO NECESITAMOS LDXR/STXR:
 *   Solo el proceso que tiene el lock puede llamar a spin_unlock().
 *   No hay competencia, asi que una escritura normal (con barreras) basta.
 * =============================================================
 */
spin_unlock:
    /*
     * DMB = Data Memory Barrier (antes de liberar)
     * 
     * DMB sy
     *   Fuerza que todas las operaciones de memoria de la seccion critica
     *   se completen ANTES de liberar el lock.
     * 
     * POR QUE ES CRITICO:
     *   Sin esta barrera, el siguiente escenario es posible:
     *   
     *   SIN DMB:
     *     buffer[0] = 42;           ← Escritura en seccion critica
     *     spin_unlock(&lock);       ← CPU reordena: libera primero!
     *     (otro proceso entra)
     *     x = buffer[0];            ← Lee valor antiguo (42 no esta escrito aun)
     *   
     *   CON DMB:
     *     buffer[0] = 42;           ← Se completa PRIMERO
     *     dmb sy;                   ← Barrera de sincronizacion
     *     spin_unlock(&lock);       ← Luego se libera
     *     (otro proceso entra)
     *     x = buffer[0];            ← Lee 42 correctamente
     * 
     * SEMANTICA "RELEASE":
     *   DMB implementa "release semantics": todas las operaciones previas
     *   son visibles para otros cores antes de que el lock se libere.
     */
    dmb sy
    
    /*
     * STLR = Store-Release Register
     * 
     * STLR wzr, [x0]
     *   wzr = "Zero Register" (registro de 32 bits que siempre vale 0)
     *   Escribe 0 en la direccion x0 (liberar el lock)
     * 
     * VENTAJAS DE STLR:
     *   1. Combina STR + barrera de memoria "release" en una instruccion
     *   2. Mas eficiente que STR + DMB separados
     *   3. Garantiza "release semantics" por hardware
     * 
     * ALTERNATIVA (equivalente pero menos eficiente):
     *   dmb sy
     *   str wzr, [x0]
     * 
     * STLR es la forma "idiomatica" de liberar locks en ARM64.
     * 
     * SEMANTICA:
     *   - Todas las lecturas/escrituras previas son visibles antes del STLR
     *   - Lecturas/escrituras POSTERIORES no pueden reordenarse ANTES del STLR
     *   - Pero escrituras posteriores SI pueden reordenarse ENTRE SI
     * 
     * POR QUE wzr Y NO UN MOV EXPLICITO:
     *   wzr es un registro especial que ahorra una instruccion:
     *   
     *   Forma larga:
     *     mov w1, #0
     *     stlr w1, [x0]
     *   
     *   Forma corta:
     *     stlr wzr, [x0]    ← wzr siempre es 0
     * 
     * ESTADO FINAL:
     *   - El lock ahora vale 0 (libre)
     *   - Otros procesos esperando en spin_lock() pueden adquirirlo
     *   - Todas las operaciones de la seccion critica son visibles
     */
    stlr wzr, [x0]
    
    /*
     * Retornar al codigo que libero el lock
     * 
     * A partir de aqui, ya no tenemos acceso exclusivo.
     * Otros procesos pueden adquirir el lock inmediatamente.
     */
    ret