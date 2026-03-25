#include <stdio.h>
#include "elf.h"
#include "linux_boot.h"
#include "sbi.h"

int linux_boot(Memory *mem, CPU *cpu, const char *kernel_path, const char *dtb_path) {
  u64 entry = elf_load(mem, kernel_path);
  if (entry == (u64)-1) {
    fprintf(stderr, "linux_boot: failed to load kernel %s\n", kernel_path);
    return -1;
  }

  if (!mem_load_file(mem, dtb_path, LINUX_DTB_ADDR)) {
    fprintf(stderr, "linux_boot: failed to load DTB %s\n", dtb_path);
    return -1;
  }

  cpu->regs[10] = 0;              // a0 = hart ID
  cpu->regs[11] = LINUX_DTB_ADDR; // a1 = DTB address
  sbi_init(cpu, entry);           // set up delegation, drop to S-mode at kernel entry
  return 0;
}
