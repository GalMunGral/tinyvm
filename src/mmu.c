#include <stdio.h>
#include "mmu.h"
#include "trap.h"

// ---------------------------------------------------------------------------
// Page table geometry
// ---------------------------------------------------------------------------
#define PAGE_OFFSET_BITS 12 // bits to address a byte within a 4KB page
#define PT_INDEX_BITS 9     // bits to index a PTE within a page table (512 entries = 2^9)
#define PT_LEVELS 3         // number of page table levels in SV39
#define PTE_SIZE 8          // each PTE is one u64

// ---------------------------------------------------------------------------
// SATP fields
// ---------------------------------------------------------------------------
#define SATP_PPN_MASK 0x00000FFFFFFFFFFFULL // bits[43:0]

// ---------------------------------------------------------------------------
// PTE flags
// ---------------------------------------------------------------------------
#define PTE_V (1ULL << 0) // valid
#define PTE_R (1ULL << 1) // readable
#define PTE_W (1ULL << 2) // writable
#define PTE_X (1ULL << 3) // executable
#define PTE_U (1ULL << 4) // user-accessible
#define PTE_A (1ULL << 6) // accessed
#define PTE_D (1ULL << 7) // dirty

#define PTE_PPN_SHIFT 10 // PPN lives at bits[53:10]

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------
static u32 fault_cause(MmuAccess access) {
  switch (access) {
  case MMU_FETCH:
    return EXC_CAUSE_FETCH_PAGE_FAULT;
  case MMU_STORE:
    return EXC_CAUSE_STORE_PAGE_FAULT;
  default:
    return EXC_CAUSE_LOAD_PAGE_FAULT;
  }
}

// Extract the VPN index for a given level from a virtual address
static u64 vpn_of(u64 va, int level) {
  u64 shift = PAGE_OFFSET_BITS + (u64)level * PT_INDEX_BITS;
  u64 mask  = (1ULL << PT_INDEX_BITS) - 1;
  return (va >> shift) & mask;
}

// Compute the physical address of a PTE given the page table's PPN and the VPN index
static u64 pte_addr_of(u64 ppn, u64 vpn) { return (ppn << PAGE_OFFSET_BITS) + vpn * PTE_SIZE; }

static bool pte_check(u64 pte, const CPU *cpu, MmuAccess access) {
  switch (access) {
  case MMU_FETCH:
    if (!(pte & PTE_X))
      return false;
    break;
  case MMU_LOAD:
    if (!(pte & PTE_R))
      return false;
    break;
  case MMU_STORE:
    if (!(pte & PTE_W))
      return false;
    break;
  }
  if (cpu->privilege == PRIV_U && !(pte & PTE_U))
    return false;
  // S-mode can access user pages only when sstatus.SUM is set
  if (cpu->privilege == PRIV_S && (pte & PTE_U) &&
      !(cpu->csrs[CSR_MSTATUS] & SSTATUS_SUM))
    return false;
  return true;
}

static u64 pte_to_pa(u64 pte, u64 va, int level) {
  u64 offset_bits = PAGE_OFFSET_BITS + (u64)level * PT_INDEX_BITS;
  u64 offset_mask = (1ULL << offset_bits) - 1;
  u64 ppn         = (pte >> PTE_PPN_SHIFT) & SATP_PPN_MASK;
  return (ppn << PAGE_OFFSET_BITS) | (va & offset_mask);
}

static bool mmu_active(const CPU *cpu) {
  return cpu->privilege != PRIV_M && (cpu->csrs[CSR_SATP] >> SATP_MODE_SHIFT) == SATP_MODE_SV39;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
u64 mmu_translate(CPU *cpu, const Memory *mem, u64 va, MmuAccess access) {
  if (!mmu_active(cpu))
    return va;

  u64 ppn      = cpu->csrs[CSR_SATP] & SATP_PPN_MASK;
  u64 pte      = 0;
  u64 pte_addr = 0;
  int level;

  for (level = PT_LEVELS - 1; level >= 0; level--) {
    pte_addr = pte_addr_of(ppn, vpn_of(va, level));
    pte      = mem_read64(mem, pte_addr);

    if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W))) {
      cpu_trap(cpu, fault_cause(access), va);
      return MMU_FAULT;
    }

    if ((pte & PTE_R) || (pte & PTE_X))
      break; // leaf found

    ppn = (pte >> PTE_PPN_SHIFT) & SATP_PPN_MASK;
  }

  if (level < 0) {
    cpu_trap(cpu, fault_cause(access), va);
    return MMU_FAULT;
  }

  if (!pte_check(pte, cpu, access)) {
    cpu_trap(cpu, fault_cause(access), va);
    return MMU_FAULT;
  }

  // Set A on all accesses, D on stores
  pte |= PTE_A;
  if (access == MMU_STORE)
    pte |= PTE_D;
  mem_write64(mem, pte_addr, pte);

  return pte_to_pa(pte, va, level);
}
