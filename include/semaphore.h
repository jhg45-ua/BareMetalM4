/**
 * @file semaphore.h
 * @brief Interfaz de semaforos para sincronizacion entre procesos
 * 
 * @details
 *   Define operaciones classicas de semaforos (Dijkstra):
 *   - wait() / P() - Esperar a que recurso este disponible
 *   - signal() / V() - Liberar recurso
 * 
 *   PROBLEMA QUE RESUELVE:
 *   Los semaforos sincroniz multiprocesos en recursos compartidos.
 *   Ejemplo: productor-consumidor, acceso a buffers criticos.
 * 
 *   SEMANTICA:
 *   - count > 0: Recurso disponible
 *   - count = 0: Recurso ocupado, procesos esperan
 *   - count < 0: Procesos bloqueados esperando (valor absoluto = numero)
 * 
 * @author Sistema Operativo Educativo
 * @version 1.0
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

/* ========================================================================== */
/* ESTRUCTURA DE SEMAFORO                                                    */
/* ========================================================================== */

/**
 * @struct semaphore
 * @brief Semaforo binario/contador para sincronizacion
 * 
 * @details
 *   IMPLEMENTACION SIMPLIFICADA:
 *   En un kernel educativo, hacemos semaforos simples.
 *   Un kernel real tendria:
 *   - wait_queue: Lista de procesos bloqueados
 *   - waiters: Contador de procesos esperando
 *   
 *   Aqui solo tenemos count, y confiamos en que el scheduler
 *   marque procesos como BLOCKED hasta que sean despertados.
 * 
 * @note Esta es una version simplificada.
 *       Kernels reales (Linux) tienen wait queues complejas.
 */
struct semaphore {
    /**
     * @brief Contador del semaforo
     * 
     * SEMANTICA:
     *   count > 0: Recurso disponible (count = numero de instancias)
     *   count = 0: Recurso ocupado, esperar
     *   
     *   OPERACIONES:
     *   - wait(): Decrementa count. Si es 0, bloquea
     *   - signal(): Incrementa count. Despierta un proceso esperando
     * 
     * EJEMPLO (buffer circulador):
     *   @code
     *   struct semaphore empty;  // Slots vacios (inicialmente 4)
     *   struct semaphore full;   // Slots llenos (inicialmente 0)
     *   
     *   productor() {
     *       sem_wait(&empty);    // Esperar slot vacio
     *       // Producir dato
     *       sem_signal(&full);   // Avisar que hay dato
     *   }
     *   
     *   consumidor() {
     *       sem_wait(&full);     // Esperar dato
     *       // Consumir dato
     *       sem_signal(&empty);  // Avisar que slot vacio
     *   }
     *   @endcode
     * 
     * @note volatile: Evita que compilador cachee el valor
     *       (el scheduler puede modificarlo desde otro proceso)
     */
    volatile int count;
};

/* ========================================================================== */
/* FUNCIONES DE SEMAFORO                                                     */
/* ========================================================================== */

/**
 * @brief Inicializa un semaforo con un valor inicial
 * 
 * @param s Puntero al semaforo a inicializar
 * @param value Valor inicial del contador (tipicamente 0 o 1)
 * 
 * @details
 *   OPERACION BASICA:
 *   Solo copia el valor en el campo count.
 *   Debe llamarse una sola vez por cada semaforo.
 * 
 *   VALORES TIPICOS:
 *   - value = 1: Semaforo BINARIO (mutex) - solo 1 proceso a la vez
 *   - value = N: Semaforo CONTADOR - N procesos pueden acceder
 *   - value = 0: Inicio bloqueado - esperar a signal()
 * 
 * @example
 * @code
 * struct semaphore mutex;
 * sem_init(&mutex, 1);  // Semaforo binario (1 recurso)
 * 
 * struct semaphore buffer;
 * sem_init(&buffer, 4); // Semaforo contador (4 recursos)
 * @endcode
 * 
 * @see sem_wait() para esperar
 * @see sem_signal() para liberar
 */
void sem_init(struct semaphore *s, int value);

/**
 * @brief Espera a que recurso este disponible (Operacion P)
 * 
 * @param s Puntero al semaforo a esperar
 * 
 * @return void
 * 
 * @details
 *   SEMANTICA CLASSICA (Dijkstra):
 *   P() = "Proberen" (holandés, significa "probar")
 * 
 *   ALGORITMO:
 *   @code
 *   while (true) {
 *       spin_lock(&sem_lock);
 *       
 *       if (s->count > 0) {
 *           s->count--;        // Decrementar contador
 *           spin_unlock(&sem_lock);
 *           return;           // Exito: entramos a seccion critica
 *       }
 *       
 *       spin_unlock(&sem_lock);
 *       schedule();           // No puedo entrar: ceder CPU
 *   }
 *   @endcode
 * 
 *   FLUJO:
 *   1. Si hay recurso disponible (count > 0): entra inmediatamente
 *   2. Si no (count = 0): cede CPU repetidamente hasta que haya
 *   
 *   LIMITACIONES:
 *   Esta implementacion es simplificada (busy-waiting en un loop).
 *   Un kernel real usaria una wait_queue y dormiria el proceso.
 * 
 *   EJEMPLOS:
 *   @code
 *   // Proteger seccion critica (mutex)
 *   struct semaphore mutex;
 *   sem_init(&mutex, 1);
 *   
 *   void seccion_critica() {
 *       sem_wait(&mutex);
 *       // ... codigo protegido ...
 *       sem_signal(&mutex);  // Liberar
 *   }
 *   
 *   // Buffer productor-consumidor
 *   struct semaphore empty, full;
 *   sem_init(&empty, 4);
 *   sem_init(&full, 0);
 *   
 *   void productor() {
 *       sem_wait(&empty);     // Esperar slot vacio
 *       buffer[put++] = dato;
 *       sem_signal(&full);    // Avisar dato disponible
 *   }
 *   @endcode
 * 
 * @see sem_signal() para liberar recurso
 * @see spin_lock() para atomicidad
 * @see schedule() para ceder CPU
 * 
 * @warning Puede bloquear indefinidamente si nadie llama signal()
 *          Los procesos quedarian en un loop esperando
 */
void sem_wait(struct semaphore *s);

/**
 * @brief Libera un recurso y despierta un proceso esperando (Operacion V)
 * 
 * @param s Puntero al semaforo a liberar
 * 
 * @return void
 * 
 * @details
 *   SEMANTICA CLASSICA (Dijkstra):
 *   V() = "Verhogen" (holandés, significa "incrementar")
 * 
 *   ALGORITMO:
 *   @code
 *   spin_lock(&sem_lock);
 *   s->count++;              // Incrementar contador
 *   spin_unlock(&sem_lock);
 *   // En un kernel real:
 *   //   Si hay procesos bloqueados: despertar uno (ponerlo en READY)
 *   @endcode
 * 
 *   FLUJO:
 *   1. Incrementa el contador
 *   2. Si habia procesos esperando: uno de ellos puede continuar
 *   3. El desbloqueado pasa a estado READY
 *   4. El scheduler lo ejecutara cuando le corresponda
 * 
 *   LIMITACIONES:
 *   Esta version es simplificada y no disperta explicitamente procesos.
 *   Confia en que: cuando hagas sem_signal(), el proceso en sem_wait()
 *   nota que count > 0 y retorna del loop.
 * 
 *   Un kernel real tendria:
 *   @code
 *   void sem_signal(struct semaphore *s) {
 *       spin_lock(&sem_lock);
 *       s->count++;
 *       
 *       if (s->waiters > 0) {
 *           proceso *p = wait_queue_pop(&s->queue);
 *           p->state = PROCESS_READY;  // Despertar
 *           s->waiters--;
 *       }
 *       spin_unlock(&sem_lock);
 *   }
 *   @endcode
 * 
 * @example
 * @code
 * struct semaphore mutex;
 * sem_init(&mutex, 1);
 * 
 * void productor() {
 *     // ... generar dato ...
 *     sem_signal(&mutex);  // Liberar
 * }
 * @endcode
 * 
 * @see sem_wait() para esperar recurso
 * @see spin_unlock() para atomicidad
 * 
 * @note Es seguro llamar signal() incluso si nadie esta esperando
 *       Solo incrementa el contador para futuras esperas
 */
void sem_signal(struct semaphore *s);

#endif // SEMAPHORE_H