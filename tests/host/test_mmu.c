#include <assert.h>
#include <stdio.h>

#include "cpu.h"
#include "memory.h"
#include "mmu.h"

// Layout: 4 pages of RAM starting at RAM_BASE
//   page 0 (L2_PA): L2 page table
//   page 1 (L1_PA): L1 page table
//   page 2 (L0_PA): L0 page table
//   page 3 (DATA_PA): data page
//
// Mapping: VA 0x1000 → DATA_PA
//   VPN[2] = 0, VPN[1] = 0, VPN[0] = 1

#define RAM_BASE 0x80000000ULL
#define PAGE 4096
#define L2_PA (RAM_BASE + 0 * PAGE)
#define L1_PA (RAM_BASE + 1 * PAGE)
#define L0_PA (RAM_BASE + 2 * PAGE)
#define DATA_PA (RAM_BASE + 3 * PAGE)

#define TEST_VA 0x1000ULL

// PTE encoding: PPN lives at bits[53:10]
#define MAKE_PTE(pa, flags) ((((pa) >> 12) << 10) | (flags))

// Pointer PTE: V=1, R=0, X=0 — points to next-level table
#define PTE_POINTER(pa) MAKE_PTE(pa, 0x1)
// Leaf PTE: V=1, R=1, W=1, A=1, D=1 — read/write data page
#define PTE_LEAF_RW(pa) MAKE_PTE(pa, 0xC7)
// Leaf PTE: V=1, R=1, W=0, A=1, D=0 — read-only
#define PTE_LEAF_RO(pa) MAKE_PTE(pa, 0x43)

#define SATP_SV39(root_pa) ((8ULL << 60) | ((root_pa) >> 12))

static void setup(Memory *mem, CPU *cpu) {
  // Wire up 3-level page table: L2[0] → L1[0] → L0[1] → DATA_PA
  mem_write64(mem, L2_PA + 0 * 8, PTE_POINTER(L1_PA));
  mem_write64(mem, L1_PA + 0 * 8, PTE_POINTER(L0_PA));
  mem_write64(mem, L0_PA + 1 * 8, PTE_LEAF_RW(DATA_PA));

  cpu->privilege      = PRIV_S;
  cpu->csrs[CSR_SATP] = SATP_SV39(L2_PA);
}

int main(void) {
  Memory *mem = mem_create();
  mem_add_region(mem, RAM_BASE, 4 * PAGE);
  CPU *cpu = cpu_create();
  setup(mem, cpu);

  // 1. Happy path — offset within the page is preserved
  u64 pa = mmu_translate(cpu, mem, TEST_VA + 0x42, MMU_LOAD);
  assert(pa == DATA_PA + 0x42);
  printf("PASS: VA 0x%llx -> PA 0x%llx\n", TEST_VA + 0x42, pa);

  // 2. Unmapped VA — no PTE at L0[2], should fault
  u64 fault = mmu_translate(cpu, mem, 0x2000, MMU_LOAD);
  assert(fault == MMU_FAULT);
  printf("PASS: unmapped VA -> fault\n");

  // 3. Permission fault — map a read-only page then try a store
  mem_write64(mem, L0_PA + 3 * 8, PTE_LEAF_RO(DATA_PA));
  u64 ro_fault = mmu_translate(cpu, mem, 0x3000, MMU_STORE);
  assert(ro_fault == MMU_FAULT);
  printf("PASS: store to read-only page -> fault\n");

  cpu_destroy(cpu);
  mem_destroy(mem);
  return 0;
}