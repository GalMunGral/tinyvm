#pragma once

#include "cpu.h"
#include "memory.h"
#include "types.h"

// ELF64 magic
#define ELF_MAGIC                                                                                  \
  "\x7f"                                                                                           \
  "ELF"
#define ELF_MAGIC_LEN 4
#define ELF_CLASS64 2  // 64-bit
#define ELF_DATA_LSB 1 // little-endian
#define ELF_MACH_RISCV 243

// ELF segment types
#define ELF_PT_LOAD 1

typedef struct {
  u8  ident[16];
  u16 type, machine;
  u32 version;
  u64 entry, ph_off, sh_off;
  u32 flags;
  u16 eh_size, ph_ent_size, ph_num;
  u16 sh_ent_size, sh_num, sh_str_idx;
} ElfHeader;

typedef struct {
  u32 type, flags;
  u64 offset, vaddr, paddr, file_size, mem_size, align;
} ElfProgramHeader;

// Load an ELF64 binary into guest memory.
// phys_base is added to each segment's paddr to get the physical load address.
// For position-dependent ELFs where paddr == physical address, pass 0.
// For the Linux kernel where paddr is relative to RAM start, pass the RAM base (e.g. 0x80000000).
// Returns the physical entry point, or (u64)-1 on failure.
u64 elf_load(Memory *mem, const char *path, u64 phys_base);

// Load an ELF64 binary and set cpu->pc to its entry point.
// Returns 0 on success, -1 on failure.
int elf_boot(Memory *mem, CPU *cpu, const char *path);