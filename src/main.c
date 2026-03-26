#include <stdio.h>
#include <string.h>

#include "clint.h"
#include "cpu.h"
#include "elf.h"
#include "linux_boot.h"
#include "memory.h"
#include "uart.h"

#define RAM_BASE 0x80000000ULL
#define RAM_SIZE (256 * 1024 * 1024) // 256MB, covers 0x80000000–0x90000000

static Memory g_mem;
static CPU    g_cpu;

static int boot(const char *mode, const char *binary) {
  if (strcmp(mode, "linux") == 0)
    return linux_boot(&g_mem, &g_cpu, binary, "dtb/tinyvm.dtb", "rootfs/initramfs.cpio.gz");
  if (strcmp(mode, "elf") == 0)
    return elf_boot(&g_mem, &g_cpu, binary);
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

  mem_add_region(&g_mem, RAM_BASE, RAM_SIZE);
  uart_init(&g_mem);
  clint_init(&g_mem, &g_cpu);
  cpu_init(&g_cpu);

  if (boot(argv[1], argv[2]) != 0)
    return 1;

  while (!g_cpu.halted) {
    cpu_step(&g_cpu, &g_mem);
    if (g_cpu.steps % 10000000 == 0)
      fprintf(stderr, "[%llu Msteps] pc=0x%llx\n", (unsigned long long)(g_cpu.steps / 1000000),
              (unsigned long long)g_cpu.pc);
  }
}