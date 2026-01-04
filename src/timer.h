#ifndef TIMER_H
#define TIMER_H

/* GIC (Interrupt Controller) */
#define GICD_BASE 0x08000000
#define GICC_BASE 0x08010000

#define GICD_CTLR      ((volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_ISENABLER ((volatile uint32_t *)(GICD_BASE + 0x100))
#define GICC_CTLR      ((volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR       ((volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_IAR       ((volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR      ((volatile uint32_t *)(GICC_BASE + 0x010))

extern void timer_set_tval(unsigned long);
extern void timer_set_ctl(unsigned long);
extern void set_vbar_el1(void *);
extern void enable_interrupts(void);
extern void vectors(void);
extern void schedule(void);

#define TIMER_INTERVAL 2000000

void timer_init();
void handle_timer_irq();

#endif // TIMER_H