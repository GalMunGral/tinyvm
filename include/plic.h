#pragma once

#include "cpu.h"
#include "memory.h"

#define PLIC_BASE 0x0C000000ULL
#define PLIC_SIZE 0x04000000ULL // 64MB region (standard virt board layout)

#define PLIC_IRQ_UART 1

void plic_init(Memory *mem, CPU *cpu);
void plic_set_pending(u32 irq);