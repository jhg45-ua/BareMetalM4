#ifndef MALLOC_H
#define MALLOC_H

#include "../types.h"

void kheap_init(unsigned long start_addr, unsigned long end_addr);

void *kmalloc(uint32_t size);

void kfree(void *ptr);

#endif // MALLOC_H