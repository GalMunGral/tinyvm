#include "exec_a.h"

// A-extension funct5 codes (bits[31:27] = funct7 >> 2)
#define F5_LR 0x02
#define F5_SC 0x03
#define F5_AMOSWAP 0x01
#define F5_AMOADD 0x00
#define F5_AMOXOR 0x04
#define F5_AMOAND 0x0C
#define F5_AMOOR 0x08
#define F5_AMOMIN 0x10
#define F5_AMOMAX 0x14
#define F5_AMOMINU 0x18
#define F5_AMOMAXU 0x1C

// funct3 width codes
#define F3_AMO_W 0x2 // 32-bit
#define F3_AMO_D 0x3 // 64-bit

// Reserved address for LR/SC — single-core so reservation always succeeds
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
  u64 addr      = cpu->regs[inst.rs1];
  s_reservation = addr;
  reg_write(cpu, inst.rd, amo_read(mem, addr, inst.funct3));
}

static void execute_sc(CPU *cpu, const Memory *mem, Instruction inst) {
  u64 addr = cpu->regs[inst.rs1];
  if (addr == s_reservation) {
    amo_write(mem, addr, cpu->regs[inst.rs2], inst.funct3);
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
  u64 addr   = cpu->regs[inst.rs1];
  u64 old    = amo_read(mem, addr, inst.funct3);
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

  amo_write(mem, addr, result, inst.funct3);
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