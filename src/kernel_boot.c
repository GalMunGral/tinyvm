#include "kernel_boot.h"

#include <stdio.h>

#include "elf.h"
#include "sbi.h"

int kernel_boot(Memory *mem, CPU *cpu, const char *kernel_path) {
  u64 entry = elf_load(mem, kernel_path, 0);
  if (entry == (u64)-1) {
    fprintf(stderr, "kernel_boot: failed to load kernel %s\n", kernel_path);
    return -1;
  }

  cpu->regs[REG_A0] = 0;  // a0 = hart ID
  cpu->regs[REG_A1] = 0;  // a1 = no DTB
  sbi_init(cpu, entry);
  return 0;
}