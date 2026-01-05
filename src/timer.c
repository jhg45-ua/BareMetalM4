/**
 * @file timer.c
 * @brief Inicializacion del timer y manejo de interrupciones
 * 
 * @details
 *   Este archivo contiene la logica para:
 *   - Inicializar el Generic Interrupt Controller (GIC)
 *   - Configurar el timer fisico ARM64
 *   - Manejar las interrupciones del timer (multitarea expropiativa)
 * 
 * @section GIC_INITIALIZATION
 *   El GIC es el controlador de interrupciones del sistema:
 *   
 *   GICD (Distributor):
 *   - Recibe IRQs de todos los dispositivos (timer, uart, etc)
 *   - Decide a que CPU enviar cada IRQ
 *   - GICD_CTLR (0x000): Habilita/deshabilita el distribuidor
 *   - GICD_ISENABLER (0x100): Habilita interrupciones especificas
 *   
 *   GICC (CPU Interface):
 *   - Interfaz de la CPU con el GIC
 *   - GICC_CTLR (0x000): Habilita interfaz de CPU
 *   - GICC_PMR (0x004): Mascara de prioridad
 *   - GICC_IAR (0x00C): Lee la IRQ pendiente
 *   - GICC_EOIR (0x010): Avisa fin de IRQ
 * 
 * @section TIMER_INTERRUPT_FLOW
 *   Flujo completo de una interrupcion del timer:
 *   @code
 *   1. Timer decrementa contador (CNTP_TVAL_EL0)
 *   2. Llega a 0 -> genera IRQ (ID 30)
 *   3. CPU salta a VBAR_EL1 + 0x280 (vector de IRQ)
 *   4. irq_handler_stub (entry.S) guarda todos los registros
 *   5. irq_handler_stub() llama a handle_timer_irq()
 *   6. handle_timer_irq() lee GICC_IAR
 *      - Obtiene ID de la interrupcion (30 = timer)
 *   7. handle_timer_irq() escribe GICC_EOIR
 *      - CRITICO: avisa al GIC que terminamos
 *   8. handle_timer_irq() recargar CNTP_TVAL_EL0
 *      - Timer estara listo para siguiente tick
 *   9. handle_timer_irq() llama a schedule()
 *      - Planificador elige siguiente proceso
 *   10. schedule() llama a cpu_switch_to()
 *       - Context switch (puede cambiar de proceso)
 *   11. irq_handler_stub() restaura registros
 *   12. ERET: vuelve al proceso (posiblemente otro)
 *   @endcode
 * 
 * @author Sistema Operativo Educativo
 * @version 0.3
 * @see timer.h para interfaz publica
 */

#include "../include/timer.h"
#include "../include/io.h"
#include "../include/types.h"

extern void timer_tick();

/* ========================================================================== */
/* INICIALIZACION DEL SISTEMA DE INTERRUPCIONES                             */
/* ========================================================================== */

/**
 * @brief Inicializa el sistema de interrupciones y el timer del sistema
 * 
 * @return void
 * 
 * @details
 *   SECUENCIA DE INICIALIZACION (ORDEN IMPORTANTE):
 *   
 *   PASO 1: Configurar tabla de vectores de excepciones
 *   - Llama a set_vbar_el1(vectors)
 *   - Le dice al CPU donde saltar cuando ocurran excepciones
 *   - DEBE ser antes de habilitar interrupciones
 *   - Si no, un interrupt causaria un crash (no hay handler)
 *   
 *   PASO 2: Inicializar el GIC (Generic Interrupt Controller)
 *   - Habilitar el distribuidor (GICD_CTLR = 1)
 *   - Habilitar interrupcion del timer (GICD_ISENABLER |= (1 << 30))
 *   - Habilitar interfaz de CPU (GICC_CTLR = 1)
 *   - Permitir todas las prioridades (GICC_PMR = 0xFF)
 *   
 *   PASO 3: Configurar el timer fisico ARM64
 *   - Establecer el intervalo (CNTP_TVAL_EL0 = TIMER_INTERVAL)
 *   - Habilitar el timer (CNTP_CTL_EL0 = 1)
 *   
 *   PASO 4: Habilitar interrupciones en el CPU
 *   - Llama a enable_interrupts()
 *   - Modifica DAIF para limpiar el bit I (IRQ mask)
 *   - A partir de aqui, el CPU recibira interrupciones
 *   
 *   VERIFICACION:
 *   Si todo esta correcto, el timer comenzara a generar IRQs
 *   cada ~104ms (TIMER_INTERVAL = 2,000,000 ticks / 19.2MHz).
 * 
 * @note Esta funcion DEBE llamarse exactamente una vez
 *       al iniciar el kernel
 * 
 * @warning Llamar ANTES de que los procesos ejecuten
 *          Si los procesos llaman enable_interrupts() pero
 *          timer_init() aun no configuro todo, ocurre crash
 * 
 * @see kernel() en kernel.c que llama esta funcion
 * @see handle_timer_irq() que se ejecuta cuando hay interrupcion
 */
void timer_init() {
    /**
     * PASO 1: Configurar la tabla de vectores de excepciones
     * 
     * set_vbar_el1(vectors)
     *   - vectors es un arreglo alineado a 2KB en vectors.S
     *   - Contiene 16 entradas, cada una apunta a un handler
     *   - Cuando ocurre una excepcion, CPU salta a:
     *     PC = VBAR_EL1 + offset (segun tipo de excepcion)
     * 
     * Para el timer:
     *   - Se genera una IRQ (ID 30)
     *   - CPU salta a VBAR_EL1 + 0x280 (vector IRQ en EL1)
     *   - Ahi hay un salto a irq_handler_stub
     * 
     * @critical: Esto DEBE ocurrir ANTES de enable_interrupts()
     */
    set_vbar_el1(vectors);

    /**
     * PASO 2: Inicializar el GIC (Generic Interrupt Controller)
     * 
     * El GIC tiene dos bloques: Distribuidor (GICD) e Interfaz de CPU (GICC)
     */
    
    /**
     * GICD_ISENABLER - Habilitar interrupciones especificas
     * 
     * Registro que controla que interrupciones llegan a la CPU.
     * Es un arreglo de bits, uno por cada interrupcion (0-31 en este registro).
     * 
     * *GICD_ISENABLER = (1 << 30)
     *   - Pone el bit 30 a 1
     *   - Habilita la interrupcion 30 (Timer del sistema)
     *   - Ahora el GIC dejara pasar las IRQs del timer
     * 
     * Otras interrupciones:
     *   - IRQ 0-27: SGI (Software Generated Interrupts)
     *   - IRQ 28-31: PPI (Private Peripheral Interrupts) - timer, pmu, etc
     *   - IRQ 30: Timer fisico (lo que usamos)
     *   - IRQ 31: FIQ
     */
    *GICD_ISENABLER = (1 << 30);
    
    /**
     * GICD_CTLR - Habilitar el distribuidor del GIC
     * 
     * *GICD_CTLR = 1
     *   - Pone el bit 0 a 1
     *   - Activa el distribuidor
     *   - Sin esto, ninguna IRQ llegaria al CPU (aunque estuvo habilitada)
     * 
     * NOTA: Este registro puede tener multiples bits para diferentes
     *       tipos de interrupciones (grupo 0, grupo 1, etc).
     *       Escribir 1 habilita el grupo 0 (el que usamos).
     */
    *GICD_CTLR = 1;
    
    /**
     * GICC_PMR - Mascara de prioridad de la interfaz de CPU
     * 
     * *GICC_PMR = 0xFF
     *   - 0xFF = 255 = todos los bits a 1
     *   - Permite TODAS las prioridades
     *   - Sin filtrado por prioridad
     * 
     * Rango: 0x00-0xFF
     *   - 0x00: Rechaza todas las interrupciones (mascara)
     *   - 0x80: Acepta solo prioridades de 0x00-0x7F
     *   - 0xFF: Acepta todas las prioridades
     * 
     * En nuestro kernel, el planificador decide la prioridad de procesos.
     * El GIC no filtra, solo entrega todas las interrupciones al CPU.
     */
    *GICC_PMR = 0xFF;
    
    /**
     * GICC_CTLR - Habilitar la interfaz de CPU del GIC
     * 
     * *GICC_CTLR = 1
     *   - Pone el bit 0 a 1
     *   - Activa la interfaz de CPU
     *   - Sin esto, el CPU no recibe las interrupciones del distribuidor
     * 
     * Secuencia:
     *   1. GICD_ISENABLER = (1 << 30)  ← El distribuidor conoce la IRQ
     *   2. GICD_CTLR = 1               ← El distribuidor esta activo
     *   3. GICC_CTLR = 1               ← La CPU puede recibir
     *   Sin pasos 2 y 3, las interrupciones nunca llegan al CPU.
     */
    *GICC_CTLR = 1;

    /**
     * PASO 3: Configurar el timer fisico ARM64
     * 
     * El timer tiene varios registros en el CP15 system (no MMIO):
     *   - CNTFRQ_EL0: Frecuencia del timer (19.2MHz en RPi4)
     *   - CNTP_TVAL_EL0: Valor de timeout (cuanto esperar)
     *   - CNTP_CTL_EL0: Control (habilitar/deshabilitar)
     *   - CNTP_CVAL_EL0: Valor comparador (se calcula internamente)
     */
    
    /**
     * timer_set_tval(TIMER_INTERVAL)
     *   - Escribir el intervalo deseado en CNTP_TVAL_EL0
     *   - TIMER_INTERVAL = 2,000,000 ticks
     *   - Esto carga el contador interno para esperar esa cantidad de ciclos
     * 
     * CALCULO:
     *   - Frecuencia: 19.2 MHz = 19,200,000 ciclos/segundo
     *   - Ticks: 2,000,000
     *   - Tiempo: 2,000,000 / 19,200,000 = 0.104 segundos = 104 ms
     * 
     * MECANISMO INTERNO:
     *   Escribir TVAL carga: CVAL = (contador_actual + TVAL)
     *   El timer decrementa constantemente desde contador_actual.
     *   Cuando contador >= CVAL, genera IRQ.
     */
    timer_set_tval(TIMER_INTERVAL);
    
    /**
     * timer_set_ctl(1)
     *   - Escribe 1 en CNTP_CTL_EL0
     *   - Bit 0 = ENABLE: Habilita el timer
     *   - Sin esto, el timer no cuenta (esta dormido)
     * 
     * REGISTRO CNTP_CTL_EL0:
     *   Bit 0: ENABLE (0=disabled, 1=enabled)
     *   Bit 1: IMASK (0=permit IRQ, 1=mask IRQ)
     *   Bit 2: ISTATUS (Read-only: 1 si hay pending IRQ)
     */
    timer_set_ctl(1);

    /**
     * PASO 4: Habilitar interrupciones en el CPU
     * 
     * enable_interrupts()
     *   - Modifica el registro DAIF (Debug/SError/IRQ/FIQ mask)
     *   - Limpia el bit I (IRQ mask bit)
     *   - A partir de aqui, el CPU aceptara IRQs del GIC
     * 
     * ORDEN CRITICA:
     *   Si haces enable_interrupts() ANTES de configurar todo:
     *   1. Llega una IRQ
     *   2. CPU salta a VBAR_EL1 + 0x280
     *   3. Si VBAR_EL1 no fue configurado -> RAM invalida -> CRASH
     *   4. Si GIC no esta inicializado -> lecturas invalidas -> comportamiento indefinido
     * 
     * Por eso DEBE ir al final de timer_init().
     */
    enable_interrupts();
}

/* ========================================================================== */
/* MANEJADOR DE INTERRUPCIONES DEL TIMER                                    */
/* ========================================================================== */

/**
 * @brief Maneja una interrupcion del timer (se ejecuta cada ~104ms)
 * 
 * @return void
 * 
 * @details
 *   PROPOSITO:
 *   - Reconocer la interrupcion del timer
 *   - Avisar al GIC que terminamos
 *   - Reconfigurar el timer para siguiente tick
 *   - Llamar al scheduler para cambio de contexto
 * 
 *   FLUJO:
 *   
 *   1. LEER GICC_IAR (Interrupt Acknowledge Register)
 *      Esto obtiene el ID de la interrupcion pendiente.
 *      Tambien informa al GIC que estamos procesandola.
 *   
 *   2. EXTRAER ID DE LA INTERRUPCION
 *      El ID esta en bits [9:0] (& 0x3FF).
 *      Valor esperado: 30 (timer fisico).
 *   
 *   3. ESCRIBIR GICC_EOIR (End of Interrupt Register)
 *      Este paso es CRITICO.
 *      Si no lo hacemos, el GIC cree que aun estamos procesando.
 *      No podra enviar la siguiente IRQ.
 *      -> MULTITAREA SE CONGELARIA <-
 *   
 *   4. SI ES LA IRQ DEL TIMER (id == 30):
 *      - Recargar el contador: timer_set_tval(TIMER_INTERVAL)
 *      - Llamar al scheduler: schedule()
 *   
 *   TIMING CRITICO:
 *   - Esta funcion debe ser RAPIDA
 *   - Se ejecuta cada ~104ms
 *   - Si es muy lenta, pasamos todo el tiempo en interrupciones
 *   - Los procesos no tendrian tiempo de ejecutarse
 * 
 * @see timer.h para GICC_IAR, GICC_EOIR
 * @see kernel.c para schedule()
 */
void handle_timer_irq() {
    /**
     * Leer Interrupt Acknowledge Register
     * 
     * uint32_t iar = *GICC_IAR;
     *   - GICC_IAR es un registro de solo lectura
     *   - Leerlo obtiene el ID de la interrupcion pendiente
     *   - Tambien informa al GIC que estamos atendiendo
     * 
     * Contenido de IAR:
     *   Bits [9:0]: ID de la interrupcion (0-1023)
     *     - 0-27: SGI (Software Generated Interrupts)
     *     - 28-31: PPI (Private Peripheral Interrupts)
     *     - 32+: SPI (Shared Peripheral Interrupts)
     *   
     *   Bits [12:10]: CPUID (cual CPU genero la IRQ, para SGI)
     *   Bits [31:13]: Reservados
     */
    uint32_t iar = *GICC_IAR;
    
    /**
     * Extraer el ID de la interrupcion
     * 
     * uint32_t id = iar & 0x3FF;
     *   - Mascara 0x3FF = 0b1111111111 (10 bits)
     *   - Extrae bits [9:0]
     *   - Descarta los bits altos (CPUID y reservados)
     * 
     * Valores esperados:
     *   30 = Timer fisico (lo que esperamos)
     *   Otros = Otros dispositivos (no procesados aqui)
     */
    uint32_t id = iar & 0x3FF;

    /**
     * AVISAR AL GIC QUE TERMINAMOS LA INTERRUPCION
     * 
     * *GICC_EOIR = iar;
     *   - Escribir el mismo valor que leimos de IAR
     *   - Informa al GIC que terminamos de procesar esta IRQ
     *   - El GIC puede ahora considerar nueva IRQ del mismo tipo
     * 
     * CRITICIDAD:
     *   Sin esta linea:
     *   - El GIC cree que aun procesamos la IRQ
     *   - No envia la siguiente IRQ del timer
     *   - El scheduler se ejecuta una sola vez -> SISTEMA CONGELADO
     * 
     *   Una de las causes mas comunes de kernels congelados es
     *   olvidar escribir EOIR.
     */
    *GICC_EOIR = iar;

    /**
     * Si es la interrupcion del timer (ID 30)
     * 
     * El GIC puede dirigir multiples interrupciones a esta funcion
     * (aunque generalmente solo el timer en este kernel simple).
     * Verificamos que sea el timer antes de procesarla.
     */
    if (id == 30) {
        /**
         * Recargar el contador del timer
         * 
         * timer_set_tval(TIMER_INTERVAL);
         *   - El timer decrement hasta 0
         *   - Cuando llega a 0, genera esta IRQ
         *   - Escribir TVAL lo reinicia para contar otros TIMER_INTERVAL ticks
         * 
         * TIMING:
         *   Sin esta linea:
         *   - El timer se queda en -1, -2, -3, ... (infinitamente negativo)
         *   - Generaria IRQs continuamente
         *   - El kernel quedaria todo el tiempo en el handler
         * 
         * Con esta linea:
         *   - El timer se reinicia cada vez
         *   - Genera una sola IRQ cada TIMER_INTERVAL ciclos
         *   - El sistema tiene tiempo para ejecutar procesos
         */
        timer_set_tval(TIMER_INTERVAL);

        timer_tick();
        
        /**
         * Llamar al scheduler para cambio de contexto
         * 
         * schedule();
         *   - Elige el siguiente proceso a ejecutar
         *   - Implementa el algoritmo de prioridades + aging
         *   - Puede llamar a cpu_switch_to() para cambiar procesos
         * 
         * MULTITAREA EXPROPIATIVA:
         *   Sin esta linea, los procesos nunca se intercambiarian.
         *   Con esta linea, cada ~104ms el scheduler elige el proximo.
         *   Los procesos corren "simultaneamente" (time-sharing).
         * 
         *   Si un proceso intenta monopolizar la CPU:
         *   1. Ejecuta durante ~104ms
         *   2. Timer genera IRQ
         *   3. Scheduler lo desplaza por otro proceso
         *   4. El kernel retoma control de la CPU
         */
        schedule();
    }
}