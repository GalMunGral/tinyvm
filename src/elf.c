#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "elf.h"

u64 elf_load(Memory *mem, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "elf_load: cannot open '%s'\n", path);
    return (u64)-1;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  rewind(f);

  u8 *data = malloc(size);
  if (!data) {
    fclose(f);
    return (u64)-1;
  }
  fread(data, 1, size, f);
  fclose(f);

  // Verify magic, 64-bit, little-endian, RISC-V
  ElfHeader *ehdr = (ElfHeader *)data;
  if (memcmp(ehdr->ident, ELF_MAGIC, ELF_MAGIC_LEN) != 0) {
    fprintf(stderr, "elf_load: not an ELF file\n");
    free(data);
    return (u64)-1;
  }
  if (ehdr->ident[4] != ELF_CLASS64 || ehdr->ident[5] != ELF_DATA_LSB ||
      ehdr->machine != ELF_MACH_RISCV) {
    fprintf(stderr, "elf_load: not a 64-bit little-endian RISC-V ELF\n");
    free(data);
    return (u64)-1;
  }

  // Load segments
  for (int i = 0; i < ehdr->ph_num; i++) {
    ElfProgramHeader *phdr = (ElfProgramHeader *)(data + ehdr->ph_off + i * ehdr->ph_ent_size);
    if (phdr->type != ELF_PT_LOAD)
      continue;

    // Copy initialized data
    if (phdr->file_size > 0)
      mem_write_buf(mem, phdr->vaddr, data + phdr->offset, phdr->file_size);

    // Zero BSS tail
    if (phdr->mem_size > phdr->file_size) {
      size_t bss_size = phdr->mem_size - phdr->file_size;
      u8    *zeros    = calloc(1, bss_size);
      mem_write_buf(mem, phdr->vaddr + phdr->file_size, zeros, bss_size);
      free(zeros);
    }
  }

  u64 entry = ehdr->entry;
  free(data);
  return entry;
}

int elf_boot(Memory *mem, CPU *cpu, const char *path) {
  u64 entry = elf_load(mem, path);
  if (entry == (u64)-1) return -1;
  cpu->pc = entry;
  return 0;
}