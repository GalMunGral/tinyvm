#include "linux_boot.h"

#include <stdio.h>

#include "elf.h"
#include "sbi.h"

int linux_boot(Memory *mem, CPU *cpu, const char *kernel_path, const char *dtb_path,
               const char *initramfs_path) {
  u64 entry = elf_load(mem, kernel_path, 0x80000000ULL);
  if (entry == (u64)-1) {
    fprintf(stderr, "linux_boot: failed to load kernel %s\n", kernel_path);
    return -1;
  }

  // HACK: when CONFIG_EFI is enabled the kernel places 2 bytes of PE/COFF "MZ"
  // magic (0x5a4d) at _start before the actual jump instruction. Skip them.
  if (mem_read16(mem, entry) == 0x5a4d)
    entry += 2;

  if (!mem_load_file(mem, dtb_path, LINUX_DTB_ADDR)) {
    fprintf(stderr, "linux_boot: failed to load DTB %s\n", dtb_path);
    return -1;
  }

  if (!mem_load_file(mem, initramfs_path, LINUX_INITRAMFS_ADDR)) {
    fprintf(stderr, "linux_boot: failed to load initramfs %s\n", initramfs_path);
    return -1;
  }

  cpu->regs[REG_A0] = 0;              // a0 = hart ID
  cpu->regs[REG_A1] = LINUX_DTB_ADDR; // a1 = DTB address
  sbi_init(cpu, entry);               // set up delegation, drop to S-mode at kernel entry
  return 0;
}
