//
// Created by julianhinojosagil on 16/1/26.
//

#include "../../include/tests.h"
#include "../../include/drivers//io.h"
#include "../../include/mm/malloc.h"
#include "../../include/kernel/kutils.h"

extern unsigned long get_sctlr_el1(void);

void test_memory() {
    unsigned long sctlr = get_sctlr_el1();
    kprintf("Estado actual de SCTLR_EL1: 0x%x\n", sctlr);

    kprintf("   [TEST] Ejecutando tests de memoria...\n");

    char *ptr = (char *)kmalloc(16);
    if (ptr) {
        k_strncpy(ptr, "TestOK", 16);
        kprintf("   [TEST] Malloc 16b: %s (Dir: 0x%x)\n", ptr, ptr);
        kfree(ptr);
    } else {
        kprintf("   [TEST] FALLO: Malloc devolvió NULL\n");
    }
}
