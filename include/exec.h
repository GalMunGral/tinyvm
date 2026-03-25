#pragma once

// Shared types and helpers for CPU execution modules (cpu.c, exec_a.c, etc.)

#include "cpu.h"
#include "memory.h"

// Decoded instruction — fields extracted from a raw 32-bit RISC-V instruction
typedef struct {
  u32 opcode;
  u32 rd, rs1, rs2;
  u32 funct3, funct7;
  i64 imm;
} Instruction;

// Write to an integer register, respecting the hardwired-zero invariant of x0
static inline void reg_write(CPU *cpu, const u32 rd, const u64 val) {
  if (rd != 0)
    cpu->regs[rd] = val;
}