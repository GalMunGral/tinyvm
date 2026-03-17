#include <stdio.h>
#include "cpu.h"
#include "memory.h"

#define RAM_SIZE (128 * 1024 * 1024) // 128MB
#define RAM_BASE 0x80000000ULL

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: tinyvm <binary>\n");
    return 1;
  }

  Memory *mem = mem_create();
  mem_add_region(mem, RAM_BASE, RAM_SIZE);

  CPU *cpu = cpu_create();

  if (!mem_load_file(mem, argv[1], RAM_BASE)) {
    fprintf(stderr, "failed to load %s\n", argv[1]);
    return 1;
  }

  while (1)
    cpu_step(cpu, mem);
}