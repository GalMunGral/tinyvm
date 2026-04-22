#pragma once

#include <stdint.h>

#include "cpu.h"
#include "memory.h"

// Access types — determine which page fault cause is raised on failure
typedef enum {
  MMU_FETCH = 0, // instruction fetch  — page fault cause 12
  MMU_LOAD  = 1, // data load          — page fault cause 13
  MMU_STORE = 2, // data store         — page fault cause 15
} MmuAccess;

// Returned by mmu_translate on page fault (cpu_trap already called, pc updated)
#define MMU_FAULT UINT64_MAX

// Translate a virtual address to a physical address.
// Returns the physical address, or MMU_FAULT if a page fault was taken.
// Pass-through (returns va unchanged) when in M-mode or SATP.MODE is bare.
u64 mmu_translate(CPU *cpu, const Memory *mem, u64 va, MmuAccess access);

// Virtual memory read/write — translate + access in one call.
// Return false on page fault (trap already taken), true on success.
bool vm_read8(CPU *cpu, const Memory *mem, u64 va, u8 *out);
bool vm_read16(CPU *cpu, const Memory *mem, u64 va, u16 *out);
bool vm_read32(CPU *cpu, const Memory *mem, u64 va, u32 *out);
bool vm_read64(CPU *cpu, const Memory *mem, u64 va, u64 *out);
bool vm_write8(CPU *cpu, const Memory *mem, u64 va, u8 val);
bool vm_write16(CPU *cpu, const Memory *mem, u64 va, u16 val);
bool vm_write32(CPU *cpu, const Memory *mem, u64 va, u32 val);
bool vm_write64(CPU *cpu, const Memory *mem, u64 va, u64 val);