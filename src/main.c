#include <stdio.h>
#include <string.h>
#include "clint.h"
#include "cpu.h"
#include "elf.h"
#include "linux_boot.h"
#include "memory.h"
#include "uart.h"

#define RAM_BASE 0x7f000000ULL
#define RAM_SIZE (256 * 1024 * 1024) // 256MB, covers 0x7f000000–0x8f000000

static int boot(Memory *mem, CPU *cpu, const char *mode, const char *binary) {
  if (strcmp(mode, "linux") == 0)
    return linux_boot(mem, cpu, binary, "dtb/tinyvm.dtb", "rootfs/initramfs.cpio.gz");
  if (strcmp(mode, "elf") == 0)
    return elf_boot(mem, cpu, binary);
  fprintf(stderr, "unknown mode: %s\n", mode);
  return -1;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: tinyvm <mode> <binary>\n");
    fprintf(stderr, "  linux <kernel.elf>  -- load kernel + DTB, Linux boot convention\n");
    fprintf(stderr, "  elf   <binary.elf>  -- load ELF and jump to entry\n");
    return 1;
  }

  Memory *mem = mem_create();
  mem_add_region(mem, RAM_BASE, RAM_SIZE);
  uart_init(mem);
  clint_init(mem);

  CPU *cpu = cpu_create();

  if (boot(mem, cpu, argv[1], argv[2]) != 0)
    return 1;

  while (!cpu->halted)
    cpu_step(cpu, mem);
}