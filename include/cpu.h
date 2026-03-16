#pragma once

#include "types.h"
#include "memory.h"

#define NUM_REGS 32

typedef struct CPU CPU;

CPU *cpu_create(void);
void cpu_destroy(CPU *cpu);
void cpu_step(CPU *cpu, Memory *mem);
void cpu_dump(const CPU *cpu);