#include "clint.h"

typedef struct {
  CPU *cpu;
  u64  mtimecmp;
} Clint;

// mtimecmp initialized to UINT64_MAX so no timer fires until software programs it
static Clint g_clint = {.mtimecmp = UINT64_MAX};

// Called every cpu_step — mirrors the CLINT hardware comparator running continuously.
// Raises or lowers STIP depending on whether mtime has reached mtimecmp.
static void clint_irq_source(CPU *cpu) {
  if (cpu->steps >= g_clint.mtimecmp)
    cpu_raise_irq(cpu, MIP_STIP);
  else
    cpu_lower_irq(cpu, MIP_STIP);
}

static u64 clint_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)width;
  if (offset == CLINT_MTIME_OFF)
    return g_clint.cpu->steps;
  if (offset == CLINT_MTIMECMP_OFF)
    return g_clint.mtimecmp;
  return 0;
}

static void clint_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)width;
  if (offset == CLINT_MTIMECMP_OFF)
    g_clint.mtimecmp = val;
  // mtime is read-only — writes ignored
}

void clint_set_timecmp(const u64 val) { g_clint.mtimecmp = val; }

void clint_init(Memory *mem, CPU *cpu) {
  g_clint.cpu = cpu;
  cpu_add_irq_source(clint_irq_source);
  mem_add_device(mem, CLINT_BASE, CLINT_SIZE, clint_read, clint_write, NULL);
}