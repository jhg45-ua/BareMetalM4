/* src/timer.c */
#include "timer.h"
#include "io.h"
#include "types.h"

void timer_init() {
    /* 1. Vectores */
    set_vbar_el1(vectors);

    /* 2. GIC Init */
    *GICD_ISENABLER = (1 << 30); // Enable ID 30 (Timer)
    *GICD_CTLR = 1;              // Enable Distributor
    *GICC_PMR = 0xFF;            // Priority Mask (Allow all)
    *GICC_CTLR = 1;              // Enable CPU Interface

    /* 3. Timer Init */
    timer_set_tval(TIMER_INTERVAL);
    timer_set_ctl(1); 

    /* 4. Enable CPU Interrupts */
    enable_interrupts();
}

void handle_timer_irq() {
    uint32_t iar = *GICC_IAR;
    uint32_t id = iar & 0x3FF;

    /* AVISAR FIN DE INTERRUPCIÓN ANTES DE NADA */
    *GICC_EOIR = iar;

    if (id == 30) {
        timer_set_tval(TIMER_INTERVAL);
        schedule(); /* Expropiación */
    }
}