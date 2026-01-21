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
 * @author Sistema Operativo Educativo BareMetalM4
 * @version 0.4
 */

#ifndef TIMER_H
#define TIMER_H

/* ========================================================================== */
/* GIC MEMORY MAPPED REGISTERS (Generic Interrupt Controller)               */
/* ========================================================================== */

/* Base address del GICD (Distributor) */
#define GICD_BASE 0x08000000

/* GICD Control Register - Habilita el GIC */
#define GICD_CTLR      ((volatile uint32_t *)(GICD_BASE + 0x000))

/* Interrupt Set-Enable Register - Habilita interrupciones especificas */
#define GICD_ISENABLER ((volatile uint32_t *)(GICD_BASE + 0x100))

/* Base address del GICC (CPU Interface) */
#define GICC_BASE 0x08010000

/* GICC Control Register - Habilita interfaz de CPU */
#define GICC_CTLR      ((volatile uint32_t *)(GICC_BASE + 0x000))

/* Priority Mask Register - Mascara de prioridad (0xFF = acepta todas) */
#define GICC_PMR       ((volatile uint32_t *)(GICC_BASE + 0x004))

/* Interrupt Acknowledge Register - Lee ID de la IRQ */
#define GICC_IAR       ((volatile uint32_t *)(GICC_BASE + 0x00C))

/* End of Interrupt Register - Indica fin de procesamiento de IRQ */
#define GICC_EOIR      ((volatile uint32_t *)(GICC_BASE + 0x010))

/* ========================================================================== */
/* FUNCIONES EXTERNAS (Implementadas en Assembly)                           */
/* ========================================================================== */

/* Configura el valor del timeout del timer (src/utils.S) */
extern void timer_set_tval(unsigned long);

/* Habilita/deshabilita el timer fisico (src/utils.S) */
extern void timer_set_ctl(unsigned long);

/* Configura la tabla de vectores de excepciones (src/utils.S) */
extern void set_vbar_el1(void *);

/* Habilita las interrupciones IRQ en el procesador (src/utils.S) */
extern void enable_interrupts(void);

/* Tabla de vectores de excepciones (src/vectors.S) */
extern void vectors(void);

/* Planificador de procesos (src/kernel.c) */
extern void schedule(void);

/* ========================================================================== */
/* CONSTANTES DE TIMING                                                      */
/* ========================================================================== */

/* Intervalo del timer: 2,000,000 ticks = ~104ms @ 19.2 MHz */
#define TIMER_INTERVAL 2000000

/* ========================================================================== */
/* FUNCIONES PUBLICAS                                                        */
/* ========================================================================== */

/* Inicializa el sistema de interrupciones y timer */
void timer_init(void);

/* Manejador de la interrupcion del timer */
void handle_timer_irq(void);

#endif // TIMER_H