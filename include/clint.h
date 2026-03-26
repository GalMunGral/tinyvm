#pragma once

#include "cpu.h"
#include "memory.h"

#define CLINT_BASE         0x2000000ULL  // base address of the CLINT device
#define CLINT_SIZE         0x10000ULL    // total CLINT region size

#define CLINT_MTIMECMP_OFF 0x4000ULL     // mtimecmp register offset (hart 0)
#define CLINT_MTIME_OFF    0xBFF8ULL     // mtime register offset

void clint_init(Memory *mem, CPU *cpu);
void clint_set_timecmp(const u64 val);