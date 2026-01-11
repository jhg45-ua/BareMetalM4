#ifndef MM_H
#define MM_H

/* Funciones externas de mm.S */
extern void set_ttbr0_el1(unsigned long);
extern void set_ttbr1_el1(unsigned long);
extern void set_mair_el1(unsigned long);
extern void set_tcr_el1(unsigned long);
extern void set_sctlr_el1(unsigned long);
extern unsigned long get_sctlr_el1(void);
extern void tlb_invalidate_all(void);

void mem_init();


#endif // MM_H