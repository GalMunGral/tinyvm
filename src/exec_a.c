#include "exec_a.h"

#include "mmu.h"
#include "opcodes.h"

// Reserved virtual address for LR/SC — single-core so reservation always succeeds
static u64 s_reservation = (u64)-1; // -1 = no active reservation

// ---------------------------------------------------------------------------
// Width-aware memory helpers
// ---------------------------------------------------------------------------

static u64 amo_read(const Memory *mem, const u64 addr, const u32 funct3) {
  return (funct3 == F3_AMO_W)
             ? (u64)(i64)(i32)mem_read32(mem, addr) // sign-extend 32-bit word to 64-bit
             : mem_read64(mem, addr);
}

static void amo_write(const Memory *mem, const u64 addr, const u64 val, const u32 funct3) {
  if (funct3 == F3_AMO_W)
    mem_write32(mem, addr, (u32)val);
  else
    mem_write64(mem, addr, val);
}

// ---------------------------------------------------------------------------
// LR/SC
// ---------------------------------------------------------------------------

static void execute_lr(CPU *cpu, const Memory *mem, Instruction inst) {
  u64 va = cpu->regs[inst.rs1];
  u64 pa = mmu_translate(cpu, mem, va, MMU_LOAD);
  if (pa == MMU_FAULT)
    return;
  s_reservation = va; // reserve the virtual address (single-core: always succeeds)
  reg_write(cpu, inst.rd, amo_read(mem, pa, inst.funct3));
}

static void execute_sc(CPU *cpu, const Memory *mem, Instruction inst) {
  u64 va = cpu->regs[inst.rs1];
  u64 pa = mmu_translate(cpu, mem, va, MMU_STORE);
  if (pa == MMU_FAULT)
    return;
  if (va == s_reservation) {
    amo_write(mem, pa, cpu->regs[inst.rs2], inst.funct3);
    s_reservation = (u64)-1;
    reg_write(cpu, inst.rd, 0); // 0 = success
  } else {
    reg_write(cpu, inst.rd, 1); // 1 = failure
  }
}

// ---------------------------------------------------------------------------
// AMOs — read old value, apply operation, write back, return old value in rd
// ---------------------------------------------------------------------------

static void execute_amo(CPU *cpu, const Memory *mem, Instruction inst) {
  u64 va = cpu->regs[inst.rs1];
  u64 pa = mmu_translate(cpu, mem, va, MMU_STORE);
  if (pa == MMU_FAULT)
    return;
  u64 old    = amo_read(mem, pa, inst.funct3);
  u64 src    = cpu->regs[inst.rs2];
  u32 funct5 = inst.funct7 >> 2;
  u64 result;

  switch (funct5) {
  case F5_AMOSWAP:
    result = src;
    break;
  case F5_AMOADD:
    result = old + src;
    break;
  case F5_AMOXOR:
    result = old ^ src;
    break;
  case F5_AMOAND:
    result = old & src;
    break;
  case F5_AMOOR:
    result = old | src;
    break;
  case F5_AMOMIN:
    result = (i64)old < (i64)src ? old : src;
    break;
  case F5_AMOMAX:
    result = (i64)old > (i64)src ? old : src;
    break;
  case F5_AMOMINU:
    result = old < src ? old : src;
    break;
  case F5_AMOMAXU:
    result = old > src ? old : src;
    break;
  default:
    result = old;
    break;
  }

  amo_write(mem, pa, result, inst.funct3);
  reg_write(cpu, inst.rd, old); // rd = old value (before operation)
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void execute_a(CPU *cpu, const Memory *mem, Instruction inst) {
  u32 funct5 = inst.funct7 >> 2;
  switch (funct5) {
  case F5_LR:
    execute_lr(cpu, mem, inst);
    break;
  case F5_SC:
    execute_sc(cpu, mem, inst);
    break;
  default:
    execute_amo(cpu, mem, inst);
    break;
  }
}