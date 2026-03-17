#include <stdio.h>
#include "cpu.h"
#include "memory.h"

int main(void) {
  Memory *mem = mem_create(128 * 1024 * 1024); // 128MB
  CPU    *cpu = cpu_create();

  cpu_dump(cpu);

  cpu_destroy(cpu);
  mem_destroy(mem);
  return 0;
}