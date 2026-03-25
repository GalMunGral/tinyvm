#include <time.h>
#include "clint.h"

// mtimecmp is a register inside the CLINT peripheral — not backed by RAM.
// Initialize to UINT64_MAX so no timer interrupt fires until software programs it.
static u64 s_mtimecmp = UINT64_MAX;

static u64 get_mtime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000 * 1000 * 1000 + (u64)ts.tv_nsec;
}

// Called every cpu_step — mirrors the CLINT hardware comparator running continuously.
// Raises or lowers MTIP depending on whether mtime has reached mtimecmp.
static void clint_irq_source(CPU *cpu) {
  if (get_mtime() >= s_mtimecmp)
    cpu_raise_irq(cpu, MIP_MTIP);
  else
    cpu_lower_irq(cpu, MIP_MTIP);
}

static u64 clint_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)width;
  if (offset == CLINT_MTIME_OFF)
    return get_mtime();
  if (offset == CLINT_MTIMECMP_OFF)
    return s_mtimecmp;
  return 0;
}

static void clint_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)width;
  if (offset == CLINT_MTIMECMP_OFF)
    s_mtimecmp = val;
  // mtime is read-only — writes ignored
}

void clint_set_timecmp(const u64 val) {
  s_mtimecmp = val;
}

void clint_init(Memory *mem) {
  cpu_add_irq_source(clint_irq_source);
  mem_add_device(mem, CLINT_BASE, CLINT_SIZE, clint_read, clint_write);
}