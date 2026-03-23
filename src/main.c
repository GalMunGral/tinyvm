#include <stdio.h>
#include "cpu.h"
#include "elf.h"
#include "memory.h"
#include "uart.h"

#define RAM_BASE 0x7f000000ULL
#define RAM_SIZE (256 * 1024 * 1024) // 256MB, covers 0x7f000000–0x8f000000

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: tinyvm <elf>\n");
    return 1;
  }

  Memory *mem = mem_create();
  mem_add_region(mem, RAM_BASE, RAM_SIZE);
  uart_init(mem);

  CPU *cpu = cpu_create();

  u64 entry = elf_load(mem, argv[1]);
  if (entry == (u64)-1) {
    fprintf(stderr, "failed to load %s\n", argv[1]);
    return 1;
  }
  cpu->pc = entry;

  while (1)
    cpu_step(cpu, mem);
}