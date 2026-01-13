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

#include "../../include/drivers/timer.h"
#include "../../include/drivers/io.h"
#include "../../include/types.h"

/* Definimos GICD_ISENABLER para habilitar IDs */
/* ID 0-31 están en ISENABLER0 (Offset 0x100) */
/* ID 32-63 están en ISENABLER1 (Offset 0x104) */
#define GICD_ISENABLER1 ((volatile uint32_t *)(GICD_BASE + 0x104))

extern void timer_tick();

/* Inicializa el sistema de interrupciones y el timer del sistema */
void timer_init() {
    /* PASO 1: Configurar tabla de vectores de excepciones */
    set_vbar_el1(vectors);

    /* PASO 2: Inicializar el GIC (Generic Interrupt Controller) */
    *GICD_ISENABLER = (1 << 30);  /* Habilitar IRQ 30 (timer) */

    /* Habilitar UART (ID 33) en ISENABLER1. 
       El bit es (33 - 32) = 1 */
    *GICD_ISENABLER1 = (1 << 1);

    *GICD_CTLR = 1;                /* Activar distribuidor */
    *GICC_PMR = 0xFF;              /* Permitir todas las prioridades */
    *GICC_CTLR = 1;                /* Activar interfaz de CPU */

    /* PASO 3: Configurar el timer fisico ARM64 */
    timer_set_tval(TIMER_INTERVAL);  /* Cargar intervalo */
    timer_set_ctl(1);                 /* Habilitar timer */

    /* PASO 4: Configurar UART Hardware para interrumpir */
    uart_irq_init();

    /* PASO 5: Habilitar interrupciones en el CPU */
    enable_interrupts();
}

/* Maneja una interrupcion del timer (se ejecuta cada ~104ms) */
void handle_timer_irq() {
    /* Leer Interrupt Acknowledge Register */
    uint32_t iar = *GICC_IAR;
    
    /* Extraer el ID de la interrupcion */
    uint32_t id = iar & 0x3FF;

    /* CRITICO: Avisar al GIC que terminamos */
    *GICC_EOIR = iar;

    /* Si es la interrupcion del timer (ID 30) */
    if (id == 30) {
        /* Recargar el contador del timer */
        timer_set_tval(TIMER_INTERVAL);

        /* Actualizar sistema de tiempo */
        timer_tick();
        
        /* Llamar al scheduler para cambio de contexto */
        schedule();
    } else if (id == 33) {
        /* Es el Teclado (UART) */
        uart_handle_irq();
        /* No necesitamos llamar a schedule() obligatoriamente aquí, 
           simplemente volvemos a lo que estábamos haciendo */
    }
}