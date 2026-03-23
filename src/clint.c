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

static u64 mtime_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)offset;
  (void)width;
  return get_mtime();
}

static void mtime_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)offset;
  (void)val;
  (void)width; // mtime is read-only
}

static u64 mtimecmp_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)offset;
  (void)width;
  return s_mtimecmp;
}

static void mtimecmp_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)offset;
  (void)width;
  s_mtimecmp = val;
}

void clint_init(Memory *mem) {
  cpu_add_irq_source(clint_irq_source);
  mem_add_device(mem, CLINT_MTIME_BASE, 8, mtime_read, mtime_write);
  mem_add_device(mem, CLINT_MTIMECMP_BASE, 8, mtimecmp_read, mtimecmp_write);
}
