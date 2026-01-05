/**
 * @file timer.h
 * @brief Interfaz para el subsistema de interrupciones y timer
 * 
 * @details
 *   Encabezado que define:
 *   - Registros del Generic Interrupt Controller (GIC)
 *   - Funciones de configuración del timer
 *   - Constantes de timing
 *   
 *   El GIC es el controlador de interrupciones de la plataforma ARM
 *   que dirige las IRQs desde dispositivos (como el timer) hacia la CPU.
 * 
 * @section GIC_ARCHITECTURE
 *   GIC (Generic Interrupt Controller) consta de 2 bloques:
 *   
 *   1. GICD (Distributor) - Base 0x08000000
 *      - Recibe interrupciones de todos los dispositivos
 *      - Decide a que CPU enviar cada IRQ
 *      - Controles globales del sistema de interrupciones
 *   
 *   2. GICC (CPU Interface) - Base 0x08010000
 *      - Interfaz de cada CPU con el sistema de interrupciones
 *      - Lee la IRQ actual (IAR)
 *      - Indica fin de interrupción (EOIR)
 *      - Configura prioridad (PMR)
 * 
 * @section TIMER_FLOW
 *   Flujo de una interrupción del timer:
 *   @code
 *   1. Timer decrementa contador hacia 0
 *   2. Cuando llega a 0, genera IRQ
 *   3. CPU salta a VBAR_EL1 + 0x280 (IRQ vector)
 *   4. irq_handler_stub() salva registros
 *   5. handle_timer_irq() lee IAR (Interrupt Acknowledge Register)
 *   6. Identifica que es IRQ 30 (timer)
 *   7. Escribe EOIR (End of Interrupt) para avisar al GIC
 *   8. Reconfigura timer para siguiente tick
 *   9. Llama a schedule() para cambio de contexto
 *   10. irq_handler_stub() restaura registros y hace ERET
 *   @endcode
 */

#ifndef TIMER_H
#define TIMER_H

/* ========================================================================== */
/* GIC MEMORY MAPPED REGISTERS (Generic Interrupt Controller)               */
/* ========================================================================== */

/**
 * @defgroup GIC_Distributor Distribuidor de Interrupciones (GICD)
 * @{
 * 
 * @brief Base address del GICD (Distributor)
 * 
 * Direccion base del bloque distribuidor del GIC.
 * Todos los registros GICD_* son desplazamientos desde aqui.
 */
#define GICD_BASE 0x08000000

/**
 * @brief Direccion del registro GICD_CTLR (GICD Control Register)
 * 
 * @details
 *   Registro de control del Distribuidor:
 *   - Bit 0: EnableGrp0 (Habilita interrupciones de grupo 0)
 *   - Bit 1: EnableGrp1 (Habilita interrupciones de grupo 1)
 *   
 *   Escribir 1 aqui activa el GIC. Sin esto, ninguna IRQ llegara al CPU.
 * 
 * @note Offset dentro de GICD_BASE: 0x000
 */
#define GICD_CTLR      ((volatile uint32_t *)(GICD_BASE + 0x000))

/**
 * @brief Direccion del registro GICD_ISENABLER (Interrupt Set-Enable Register)
 * 
 * @details
 *   Habilita interrupciones especificas del sistema:
 *   - Bit 30: Timer interrupt (ID 30) ← Lo que nos interesa
 *   - Otros bits: Otros dispositivos (uart, gpio, etc)
 *   
 *   Para habilitar el timer:
 *     *GICD_ISENABLER = (1 << 30);
 *   
 *   Esto pone el bit 30 a 1, habilitando la interrupcion del timer.
 *   Sin esto, el timer generaria interrupciones pero seran bloqueadas.
 * 
 * @note Offset dentro de GICD_BASE: 0x100
 * @note Escribir 1 HABILITA una interrupcion
 *       Escribir 0 no tiene efecto (usar ICENABLER para deshabilitar)
 */
#define GICD_ISENABLER ((volatile uint32_t *)(GICD_BASE + 0x100))

/** @} */ // Fin GIC_Distributor

/**
 * @defgroup GIC_CPUInterface Interfaz de CPU (GICC)
 * @{
 * 
 * @brief Base address del GICC (CPU Interface)
 * 
 * Direccion base de la interfaz de CPU del GIC.
 * Registros que cada CPU usa para comunicarse con el GIC.
 */
#define GICC_BASE 0x08010000

/**
 * @brief Direccion del registro GICC_CTLR (GICC Control Register)
 * 
 * @details
 *   Registro de control de la interfaz de CPU:
 *   - Bit 0: EnableGrp0 (Habilita grupo 0 en esta CPU)
 *   - Bit 1: EnableGrp1 (Habilita grupo 1 en esta CPU)
 *   
 *   Escribir 1 aqui activa la interfaz de CPU.
 *   Sin esto, el CPU no recibira interrupciones del GIC.
 * 
 * @note Offset dentro de GICC_BASE: 0x000
 */
#define GICC_CTLR      ((volatile uint32_t *)(GICC_BASE + 0x000))

/**
 * @brief Direccion del registro GICC_PMR (Priority Mask Register)
 * 
 * @details
 *   Mascara de prioridad:
 *   - Solo IRQs con prioridad > PMR seran aceptadas
 *   - Valor rango: 0-255 (0 = rechaza todos, 255 = acepta todos)
 *   - 0xFF (255) = Aceptar todas las interrupciones
 *   
 *   En nuestro kernel usamos 0xFF para no filtrar por prioridad.
 *   El scheduler maneja la prioridad de procesos, no el GIC.
 * 
 * @note Offset dentro de GICC_BASE: 0x004
 * @see Esta es la "puerta de entrada" para que IRQs lleguen al CPU
 */
#define GICC_PMR       ((volatile uint32_t *)(GICC_BASE + 0x004))

/**
 * @brief Direccion del registro GICC_IAR (Interrupt Acknowledge Register)
 * 
 * @details
 *   Al leer este registro:
 *   1. El GIC devuelve ID de la interrupcion que acaba de llegar
 *   2. Guarda el estado de la IRQ en la CPU
 *   
 *   Formato del valor leido:
 *   - Bits [9:0]: CPUID de quien genero la IRQ
 *   - Bits [12:10]: Reserved (0)
 *   - Bits [19:13]: Reserved (0)
 *   - Bits [29:20]: Reserved (0)
 *   - Bits [31:30]: Reserved (0)
 *   
 *   En la practica, para el timer:
 *   - iar = *GICC_IAR;
 *   - id = iar & 0x3FF;  // Extraer ID (bits [9:0])
 *   - if (id == 30) ...  // Es la interrupcion del timer
 * 
 * @note Offset dentro de GICC_BASE: 0x00C
 * @important DEBE leerse para informar al GIC que la IRQ fue procesada
 */
#define GICC_IAR       ((volatile uint32_t *)(GICC_BASE + 0x00C))

/**
 * @brief Direccion del registro GICC_EOIR (End of Interrupt Register)
 * 
 * @details
 *   Escribir aqui indica al GIC que hemos terminado de procesar una IRQ.
 *   
 *   FLUJO CORRECTO:
 *   1. Leer GICC_IAR -> obtenemos ID de la IRQ
 *   2. Procesar la interrupcion (ej: recargar timer)
 *   3. Escribir GICC_EOIR -> avisamos al GIC que terminamos
 *   
 *   SI NO ESCRIBIMOS EOIR:
 *   - El GIC cree que aun estamos procesando la IRQ
 *   - No podra enviar la siguiente IRQ del mismo tipo
 *   - Multitarea se congela (no hay cambios de contexto)
 *   
 *   DATOS A ESCRIBIR:
 *   - Escribimos el mismo valor que leimos de IAR
 *   - El GIC lo usa para identificar que IRQ termino
 * 
 * @note Offset dentro de GICC_BASE: 0x010
 * @critical DEBE escribirse en el handler de cada IRQ
 * @see handle_timer_irq() como ejemplo correcto
 */
#define GICC_EOIR      ((volatile uint32_t *)(GICC_BASE + 0x010))

/** @} */ // Fin GIC_CPUInterface

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Implementadas en Assembly)                           */
/* ========================================================================== */

/**
 * @brief Configura el valor del timeout del timer (src/utils.S)
 * 
 * @param valor_ticks Numero de ciclos del timer a esperar
 * 
 * @details
 *   Escribe en el registro CNTP_TVAL_EL0 del timer fisico.
 *   
 *   CALCULO:
 *   - Frecuencia del timer: 19.2 MHz (19,200,000 Hz) en RPi4
 *   - Para 1ms: ticks = 19200000 / 1000 = 19200
 *   - Para 2ms: ticks = 38400
 *   
 *   TIMER_INTERVAL (2000000 ticks) = ~104ms en RPi4
 *   Esto es la duracion de cada "timeslice" (tiempo que cada proceso corre)
 * 
 * @see src/utils.S para implementacion
 * @see timer_init() para uso
 */
extern void timer_set_tval(unsigned long);

/**
 * @brief Habilita/deshabilita el timer fisico (src/utils.S)
 * 
 * @param control Valor de control:
 *   - Bit 0: ENABLE (1=habilitado, 0=deshabilitado)
 *   - Bit 1: IMASK (1=maskea IRQ, 0=permite IRQ)
 *   
 *   Valores tipicos:
 *   - 0: Timer deshabilitado
 *   - 1: Timer habilitado, permitir IRQ (lo que usamos)
 *   - 3: Timer habilitado, pero maskear IRQ (debug)
 * 
 * @details
 *   Escribe en el registro CNTP_CTL_EL0 del timer fisico.
 * 
 * @see src/utils.S para implementacion
 * @see timer_init() para uso
 */
extern void timer_set_ctl(unsigned long);

/**
 * @brief Configura la tabla de vectores de excepciones (src/utils.S)
 * 
 * @param tabla_base Direccion base de la tabla de vectores
 * 
 * @details
 *   Escribe el puntero en VBAR_EL1 (Vector Base Address Register).
 *   Esto le dice al CPU donde saltar cuando ocurran excepciones.
 *   
 *   Nuestra tabla de vectores esta en vectors.S:
 *   @code
 *   set_vbar_el1(vectors);
 *   @endcode
 * 
 * @see src/utils.S para implementacion
 * @see src/vectors.S para tabla de vectores
 */
extern void set_vbar_el1(void *);

/**
 * @brief Habilita las interrupciones IRQ en el procesador (src/utils.S)
 * 
 * @details
 *   Modifica el registro DAIF para limpiar el bit I (IRQ mask).
 *   Sin esto, el CPU ignorara todas las interrupciones.
 * 
 * @see src/utils.S para implementacion
 * @see kernel() en kernel.c para uso
 */
extern void enable_interrupts(void);

/**
 * @brief Tabla de vectores de excepciones (src/vectors.S)
 * 
 * @details
 *   Arreglo de vectores que define donde saltar para cada tipo
 *   de excepcion (IRQ, SVC, errores, etc).
 *   
 *   Alineada a 2KB (requisito ARM64).
 *   Cada entrada ocupa 128 bytes.
 * 
 * @see src/vectors.S para estructura completa
 */
extern void vectors(void);

/**
 * @brief Planificador de procesos (src/kernel.c)
 * 
 * @details
 *   Elige el siguiente proceso a ejecutar.
 *   Implementa algoritmo de prioridades con aging.
 * 
 * @see src/kernel.c para detalles del algoritmo
 * @see handle_timer_irq() que lo llama
 */
extern void schedule(void);

/* ========================================================================== */
/* CONSTANTES DE TIMING                                                      */
/* ========================================================================== */

/**
 * @brief Intervalo del timer en ticks
 * 
 * @details
 *   TIMER_INTERVAL = 2,000,000 ticks
 *   
 *   CALCULO:
 *   - Frecuencia: 19.2 MHz = 19,200,000 ciclos/segundo
 *   - 2,000,000 ticks / 19,200,000 ciclos = 0.104 segundos = ~104ms
 *   
 *   SIGNIFICADO:
 *   - Cada 104ms, el timer genera una IRQ
 *   - En cada IRQ, el scheduler elige el siguiente proceso
 *   - Los procesos no pueden correr mas de 104ms sin ser interrumpidos
 *   
 *   AJUSTE:
 *   - Para timeslice mas corto: reducir TIMER_INTERVAL (ej: 1000000 = 52ms)
 *   - Para timeslice mas largo: aumentar TIMER_INTERVAL (ej: 5000000 = 260ms)
 *   
 *   TRADEOFF:
 *   - Timeslice corto: Mejor multitarea, mas context switches (overhead)
 *   - Timeslice largo: Menos overhead, pero multitarea menos fluida
 * 
 * @see timer_init() donde se usa
 * @see handle_timer_irq() donde se recarga
 */
#define TIMER_INTERVAL 2000000

/* ========================================================================== */
/* FUNCIONES PUBLICAS                                                        */
/* ========================================================================== */

/**
 * @brief Inicializa el sistema de interrupciones y timer
 * 
 * @return void
 * 
 * @details
 *   PASOS:
 *   1. Configura VBAR_EL1 (tabla de vectores)
 *   2. Inicializa el GIC (distribuidor e interfaz de CPU)
 *   3. Habilita la interrupcion del timer (ID 30)
 *   4. Configura el timer con el intervalo correcto
 *   5. Habilita interrupciones en el CPU
 *   
 *   ORDEN IMPORTANTE:
 *   - VBAR_EL1 DEBE configurarse ANTES de habilitar interrupciones
 *   - Si habilitas interrupciones sin tabla de vectores, ocurre un crash
 *   - GIC debe estar listo ANTES de enable_interrupts()
 * 
 * @note Esta funcion es llamada por kernel() en kernel.c
 * @warning Llamar solo una vez durante la inicializacion
 * @see timer.c para implementacion
 */
void timer_init(void);

/**
 * @brief Manejador de la interrupcion del timer
 * 
 * @return void
 * 
 * @details
 *   Se ejecuta cada ~104ms cuando el timer genera una IRQ.
 *   
 *   PASOS:
 *   1. Leer GICC_IAR (Interrupt Acknowledge Register)
 *      - Obtiene el ID de la IRQ (deberia ser 30 = timer)
 *   2. Escribir GICC_EOIR (End of Interrupt)
 *      - Avisar al GIC que terminamos de procesar esta IRQ
 *      - CRITICO: sin esto, se congelaria la multitarea
 *   3. Si ID == 30 (es el timer):
 *      - Recargar CNTP_TVAL_EL0 (para siguiente tick)
 *      - Llamar a schedule() (planificador)
 *   
 *   FLUJO COMPLETO:
 *   @code
 *   Timer tick (cada 104ms)
 *     |
 *     +-- CPU salta a irq_handler_stub (entry.S)
 *     |
 *     +-- irq_handler_stub() guarda registros
 *     |
 *     +-- irq_handler_stub() llama a handle_timer_irq()
 *     |
 *     +-- handle_timer_irq() lee IAR, escribe EOIR
 *     |
 *     +-- handle_timer_irq() llama a schedule()
 *     |
 *     +-- schedule() elige siguiente proceso
 *     |
 *     +-- cpu_switch_to() hace context switch
 *     |
 *     +-- irq_handler_stub() restaura registros
 *     |
 *     +-- ERET: vuelve al proceso (posiblemente otro)
 *   @endcode
 * 
 * @note Es llamada por irq_handler_stub() en entry.S
 * @see src/entry.S para el wrapper que la llama
 * @see src/kernel.c para schedule()
 * @critical DEBE escribir EOIR, sino no hay mas interrupciones
 */
void handle_timer_irq(void);

#endif // TIMER_H