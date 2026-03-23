#pragma once

#include "cpu.h"
#include "memory.h"

#define CLINT_MTIME_BASE 0x200bff8ULL    // free-running timer (read-only)
#define CLINT_MTIMECMP_BASE 0x2004000ULL // timer compare register (per hart)

void clint_init(Memory *mem);