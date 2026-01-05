/**
 * @file semaphore.c
 * @brief Implementacion de semaforos para sincronizacion entre procesos
 * 
 * @details
 *   SEMAFOROS EN ESTE KERNEL:
 * 
 *   Implementacion "simplificada" del semaforo de Dijkstra.
 *   No usa colas de espera (wait queues como en kernels reales).
 *   En su lugar, usa busy-waiting con schedule() para ceder CPU.
 * 
 *   SEMANTICA CLASSICA (Dijkstra):
 *   @code
 *   P(s):  // Entrada a seccion critica (wait)
 *       while (s.count == 0) doNothing();  // Esperar
 *       s.count--;
 * 
 *   V(s):  // Salida de seccion critica (signal)
 *       s.count++;
 *   @endcode
 * 
 *   PROTECCION CONTRA RACE CONDITIONS:
 *   - spin_lock() adquiere un spinlock atomico (LDXR/STXR)
 *   - Solo un proceso puede modificar count simultaneamente
 *   - Sin spin_lock, dos procesos podrian decrementar count al mismo tiempo:
 *     @code
 *     Proceso A: Lee count=1, va a decrementar
 *     Proceso B: Lee count=1, va a decrementar
 *     Proceso A: count = 1 - 1 = 0 (escribe)
 *     Proceso B: count = 1 - 1 = 0 (escribe, perdiose A!)
 *     Result: count=0 pero 2 procesos entraron a la seccion critica
 *     @endcode
 * 
 *   BUSY-WAITING EN LUGAR DE WAIT QUEUES:
 *   - Esperar en un bucle (while) y ceder CPU con schedule()
 *   - Cuando otro proceso hace V() (signal), simplemente retorna
 *   - No hay mecanismo para "despertar" (wake) explicitamente
 *   - Por eso el proceso que espera debe periodicamente revisar
 *   
 *   VENTAJAS:
 *   - Implementacion MUY simple (3 funciones de 1-5 lineas)
 *   - Facil de entender para estudiantes
 * 
 *   DESVENTAJAS (comparado con un kernel real):
 *   - CPU se desperdicia revisando continuamente
 *   - Sin condition variables para coordinacion eficiente
 *   - Scaling pobre (muchos procesos esperando = perdida de tiempo)
 *   
 *   KERNELS REALES USAN:
 *   - Wait queues (procesos duermen, no consumen CPU)
 *   - Explicit wake-up (V() activa un waiter)
 *   - Eventos (condition variables como en pthreads)
 * 
 * @author Sistema Operativo Educativo
 * @version 0.3
 * @see semaphore.h para interfaz publica
 */

#include "../include/semaphore.h"
#include "../include/sched.h"

extern void spin_lock(volatile int *lock);
extern void spin_unlock(volatile int *lock);
extern void schedule(void);

/**
 * @brief Spinlock global para proteger operaciones de semaforo
 * 
 * @details
 *   Como no tenemos listas de espera complejas (wait queues),
 *   usamos un spinlock global para proteger la operacion del semaforo
 *   atomicamente.
 * 
 *   En kernels reales, cada semaforo tendra su propio spinlock
 *   para evitar congestion (multiple semaforos esperando un solo lock).
 * 
 *   VALOR VOLATIL:
 *   - volatile: El compilador no cachea el valor
 *   - int: 0 = unlocked, 1 = locked (manejado por spin_lock/unlock)
 */
volatile int sem_lock = 0;

/* ========================================================================== */
/* OPERACIONES DE SEMAFORO                                                   */
/* ========================================================================== */

/**
 * @brief Inicializa un semaforo con un valor inicial
 * 
 * @param s Puntero a estructura semaphore
 * @param value Valor inicial del contador
 * 
 * @details
 *   FUNCIONAMIENTO:
 *   Simplemente copia el valor en s->count.
 * 
 *   CASOS DE USO:
 *   @code
 *   // Mutex (exclusion mutua): valor inicial = 1
 *   struct semaphore mutex;
 *   sem_init(&mutex, 1);
 *   
 *   // Barrera: valor inicial = 0
 *   // Solo cuando N procesos hagan V() se puede proceder
 *   struct semaphore barrier;
 *   sem_init(&barrier, 0);
 * 
 *   // Recursos: valor inicial = cantidad disponible
 *   struct semaphore pool;
 *   sem_init(&pool, 5);  // 5 recursos disponibles
 *   @endcode
 * 
 *   VALOR VOLATIL:
 *   El campo count es volatile porque puede cambiar entre llamadas.
 *   El compilador no debe cachear el valor en registro.
 */
void sem_init(struct semaphore *s, int value) {
    /**
     * Inicializacion simple: copiar valor inicial
     * 
     * NOTA: En kernels multicore reales, esto tambien inicializaria
     * un spinlock protegiendo acceso al semaforo. Este kernel
     * es single-core por ahora, pero la estructura ya tiene
     * proteccion con spin_lock() en wait/signal.
     */
    s->count = value;
}

/**
 * @brief Espera (P/wait): decrementa el semaforo, bloqueando si es necesario
 * 
 * @param s Puntero a estructura semaphore
 * 
 * @details
 *   ALGORITMO CLASSICO P(s):
 *   @code
 *   loop {
 *       spin_lock(&lock);
 *       if (s->count > 0) {
 *           s->count--;
 *           spin_unlock(&lock);
 *           return;  // Adquirimos recurso
 *       }
 *       spin_unlock(&lock);
 *       schedule();  // Ceder CPU, esperar a que otro incremente count
 *   }
 *   @endcode
 * 
 *   PASO 1: INTENTAR ADQUIRIR ATOMICAMENTE
 *   Adquirimos spinlock para acceso exclusivo a count.
 *   Si count > 0 (recurso disponible):
 *   - Decrementamos count (reservamos el recurso)
 *   - Liberamos spinlock
 *   - Retornamos (exito)
 * 
 *   PASO 2: BLOQUEO Y ESPERA
 *   Si count <= 0 (recurso no disponible):
 *   - Liberamos spinlock (importante! otro puede hacer signal)
 *   - Llamamos schedule() para ceder la CPU
 *   - El scheduler elige otro proceso para ejecutar
 *   - Eventualmente, schedule() nos dara la CPU nuevamente
 *   - Volvemos al paso 1
 * 
 *   RAZON DE INTENTAR DENTRO DE SPINLOCK:
 *   Evita race condition entre revisar count y decrementarlo:
 *   
 *   SIN SPINLOCK (INCORRECTO):
 *   A: if (s->count > 0)  // count=1, pasa el if
 *   B: if (s->count > 0)  // count=1, pasa el if
 *   A: s->count--         // count=0
 *   B: s->count--         // count=-1 (INCORRECTO!)
 *   
 *   CON SPINLOCK (CORRECTO):
 *   A: lock()
 *   A: if (s->count > 0) s->count--  // Atomico
 *   A: unlock()
 *   B: lock()            // B espera aqui mientras A tiene spinlock
 *   B: if (s->count > 0) ...
 * 
 *   EXAMPLE - MUTEX (Exclusion Mutua):
 *   @code
 *   struct semaphore mutex;
 *   sem_init(&mutex, 1);
 *   
 *   void seccion_critica() {
 *       sem_wait(&mutex);        // P: Adquirir
 *       // ... codigo critico ...
 *       sem_signal(&mutex);      // V: Liberar
 *   }
 *   
 *   // Solo UN proceso puede estar en seccion_critica()
 *   // Segundo proceso se bloquea en sem_wait hasta que primero hace signal
 *   @endcode
 * 
 *   EXAMPLE - PRODUCTORES/CONSUMIDORES:
 *   @code
 *   struct semaphore empty, full;
 *   sem_init(&empty, N);  // N espacios vacios en buffer
 *   sem_init(&full, 0);   // 0 items llenos
 * 
 *   void productor() {
 *       sem_wait(&empty);      // Esperar espacio vacio
 *       buffer[head++] = data;
 *       sem_signal(&full);     // Señal: hay item disponible
 *   }
 *   
 *   void consumidor() {
 *       sem_wait(&full);       // Esperar item disponible
 *       data = buffer[tail++];
 *       sem_signal(&empty);    // Señal: hay espacio vacio
 *   }
 *   @endcode
 * 
 * @warning NO USAR sem_wait() si ya se tiene el spinlock, puede deadlock
 * @critical Busy-waiting consume CPU. Kernels reales usan wait queues.
 * 
 * @see sem_signal() para incrementar y liberar
 * @see spin_lock() para adquisicion atomica del spinlock
 */
void sem_wait(struct semaphore *s) {
    /**
     * LOOP INFINITO: Intentar adquirir recurso hasta lograrlo
     * 
     * Este es el busy-wait: seguimos intentando hasta que count > 0
     */
    while (1) {
        /**
         * PASO 1: PROTEGER ACCESO A count CON SPINLOCK
         * 
         * Adquirimos spinlock para asegurar que nadie mas accede a count
         * mientras estamos revisando y potencialmente modificando.
         */
        spin_lock(&sem_lock);

        /**
         * PASO 2: REVISAR SI RECURSO ESTA DISPONIBLE
         * 
         * Si count > 0: al menos 1 recurso disponible
         * Decrementamos count (reservamos 1)
         * Liberamos spinlock
         * Retornamos (exito - no hacemos sem_wait nuevamente)
         */
        if (s->count > 0) {
            s->count--;           /**< Decrementar contador (reservar recurso) */
            spin_unlock(&sem_lock); /**< Liberar spinlock */
            return;               /**< Exito! Entramos a la seccion critica */
        }

        /**
         * PASO 3: RECURSO NO DISPONIBLE - CEDER CPU
         * 
         * count <= 0 significa que el recurso esta ocupado (otro proceso lo tiene).
         * No podemos hacer spin_lock(&sem_lock) nuevamente (deadlock).
         * En su lugar, liberamos el spinlock y cedemos la CPU con schedule().
         * 
         * schedule() permitira que otros procesos ejecuten.
         * Idealmente, aquellos que tengan count == recurso lo liberaran (sem_signal).
         * Eventualmente, schedule() nos volvera a dar CPU.
         * Revisaremos nuevamente (siguiente iteracion de while).
         * 
         * NOTA SOBRE BLOQUEO:
         * En kernels reales, marcaríamos el proceso como TASK_BLOCKED
         * para que el scheduler no lo vuelva a elegir hasta que alguien
         * lo despierte (sem_signal hace wake_up).
         * 
         * Aqui simplemente schedule() cada vez, dejando al scheduler
         * que lo siga considerando (pero con prioridad baja u otro mecanismo).
         */
        spin_unlock(&sem_lock);   /**< Liberar spinlock ANTES de ceder CPU */

        /**
         * Ceder la CPU voluntariamente
         * Otro proceso ejecutara ahora, esperemos que incremente count
         */
        schedule();
    }
}

/**
 * @brief Señal (V/signal): incrementa el semaforo, despertando waiters
 * 
 * @param s Puntero a estructura semaphore
 * 
 * @details
 *   ALGORITMO CLASSICO V(s):
 *   @code
 *   spin_lock(&lock);
 *   s->count++;
 *   spin_unlock(&lock);
 *   // (No hay wake expliciito - los waiters checan periodicamente)
 *   @endcode
 * 
 *   PASO UNICO: INCREMENTAR CONTADOR
 *   Con spinlock protegiendo, incrementamos count.
 *   Esto señala que un recurso esta disponible.
 * 
 *   NO HAY WAKE EXPLICITO:
 *   Los procesos que hacen sem_wait() estan en un loop.
 *   Revisaran count nuevamente cuando schedule() les devuelva CPU.
 *   Cuando vean count > 0, saldran del loop y continuaran.
 * 
 *   KERNELS REALES:
 *   Tendrian una lista de procesos durmiendo en el semaforo.
 *   V() despertaria uno de la lista (wake_up):
 *   @code
 *   void sem_signal(struct semaphore *s) {
 *       spin_lock(&s->lock);
 *       if (++s->count <= 0) {
 *           // Alguien estaba esperando, despertarlo
 *           struct task *waiter = list_remove(&s->wait_queue);
 *           wake_up(waiter);  // Agregar a ready queue
 *       }
 *       spin_unlock(&s->lock);
 *   }
 *   @endcode
 * 
 * @see sem_wait() para P/wait
 * @see spin_lock() para atomicidad
 */
void sem_signal(struct semaphore *s) {
    /**
     * INCREMENTAR CONTADOR ATOMICAMENTE
     * 
     * Protegido por spinlock para evitar race condition:
     * Si dos procesos hacen signal simultaneamente sin spinlock:
     *   A: Lee count=0
     *   B: Lee count=0
     *   A: count = 0 + 1 = 1 (escribe)
     *   B: count = 0 + 1 = 1 (escribe, perdemos A!)
     *   Resultado: count=1 pero deberia ser 2
     * 
     * Con spinlock, uno espera mientras el otro incrementa.
     */
    spin_lock(&sem_lock);   /**< Adquirir spinlock */
    s->count++;             /**< Incrementar contador (liberar recurso) */
    spin_unlock(&sem_lock); /**< Liberar spinlock */

    /**
     * NO HACEMOS WAKE EXPLICITO:
     * Los procesos esperando en sem_wait() estan looppeando.
     * En la siguiente llamada a schedule(), les dara CPU.
     * Revisaran count y si ahora count > 0, saldran del loop.
     * 
     * ESTO ES DIFERENTE A:
     * - pthreads (pthread_cond_signal)
     * - Linux futex (wake syscall)
     * - Windows events (SetEvent)
     * 
     * Esos si tienen mecanismos explicitios de despertar.
     * Este kernel deja que los waiters loopeen hasta encontrar count > 0.
     * 
     * EFICIENCIA:
     * Esto NO es eficiente (CPU wasted), pero es educativo y simple.
     */
}