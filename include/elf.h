#pragma once

#include "types.h"
#include "memory.h"

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
// Returns the entry point address, or (u64)-1 on failure.
u64 elf_load(Memory *mem, const char *path);