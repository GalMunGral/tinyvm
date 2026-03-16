#include <stdio.h>
#include "cpu.h"

struct CPU {
  u64 regs[NUM_REGS];
  u64 pc;
};

CPU *cpu_create(void) { return NULL; }

void cpu_destroy(CPU *cpu) {}

void cpu_step(CPU *cpu, Memory *mem) {}

void cpu_dump(const CPU *cpu) {}