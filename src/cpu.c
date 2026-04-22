#include "cpu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exec.h"
#include "exec_a.h"
#include "exec_c.h"
#include "mmu.h"
#include "opcodes.h"
#include "sbi.h"

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void execute_trap_return(CPU *cpu, const Memory *mem, i64 imm);

// ---------------------------------------------------------------------------
// Helpers — integer
// ---------------------------------------------------------------------------
static i64 sign_extend(u64 val, int bits) {
  int shift = 64 - bits;
  return (i64)(val << shift) >> shift;
}

static u64 csr_read(const CPU *cpu, u32 addr) {
  switch (addr) {
  case CSR_SSTATUS:
    return cpu->csrs[CSR_MSTATUS] & SSTATUS_MASK;
  case CSR_SIE:
    return cpu->csrs[CSR_MIE] & cpu->csrs[CSR_MIDELEG];
  case CSR_SIP:
    return cpu->csrs[CSR_MIP] & cpu->csrs[CSR_MIDELEG];
  // cycle, time, and instret all reflect the same step counter owned by the CPU
  case CSR_CYCLE:
  case CSR_TIME:
  case CSR_INSTRET:
  case CSR_MCYCLE:
  case CSR_MINSTRET:
    return cpu->steps;
  default:
    return cpu->csrs[addr];
  }
}

static void csr_write(CPU *cpu, u32 addr, u64 val) {
  switch (addr) {
  case CSR_SSTATUS:
    // Only SSTATUS_MASK bits are writable through sstatus; other mstatus bits are preserved
    cpu->csrs[CSR_MSTATUS] = (cpu->csrs[CSR_MSTATUS] & ~SSTATUS_MASK) | (val & SSTATUS_MASK);
    break;
  case CSR_SIE:
    // S-mode can only set bits that M-mode has delegated via mideleg
    cpu->csrs[CSR_MIE] =
        (cpu->csrs[CSR_MIE] & ~cpu->csrs[CSR_MIDELEG]) | (val & cpu->csrs[CSR_MIDELEG]);
    break;
  case CSR_SIP:
    // S-mode can only write SSIP (software interrupt pending); other bits are read-only from S-mode
    cpu->csrs[CSR_MIP] = (cpu->csrs[CSR_MIP] & ~MIP_SSIP) | (val & MIP_SSIP);
    break;
  case CSR_SATP: {
    // Real hardware clears unsupported mode bits to bare (0); we only support sv39 (8) and bare (0)
    u64 mode = (val & SATP_MODE_MASK) >> SATP_MODE_SHIFT;
    if (mode != SATP_MODE_BARE && mode != SATP_MODE_SV39)
      val &= ~SATP_MODE_MASK; // clear mode → bare
    cpu->csrs[CSR_SATP] = val;
    break;
  }
  default:
    cpu->csrs[addr] = val;
    break;
  }
}

// ---------------------------------------------------------------------------
// Helpers — float (memcpy avoids strict aliasing UB)
// ---------------------------------------------------------------------------
static float freg_get_f(const CPU *cpu, u32 r) {
  float v;
  memcpy(&v, &cpu->fregs[r], sizeof(float));
  return v;
}

static double freg_get_d(const CPU *cpu, u32 r) {
  double v;
  memcpy(&v, &cpu->fregs[r], sizeof(double));
  return v;
}

static void freg_set_f(CPU *cpu, u32 r, float v) { memcpy(&cpu->fregs[r], &v, sizeof(float)); }

static void freg_set_d(CPU *cpu, u32 r, double v) { memcpy(&cpu->fregs[r], &v, sizeof(double)); }

// fclass: return 10-bit classification mask per RISC-V spec
static u64 fclass_f(float v) {
  if (isinf(v) && v < 0)
    return 1 << 0;
  if (fpclassify(v) == FP_NORMAL && v < 0)
    return 1 << 1;
  if (fpclassify(v) == FP_SUBNORMAL && v < 0)
    return 1 << 2;
  if (v == 0 && signbit(v))
    return 1 << 3;
  if (v == 0 && !signbit(v))
    return 1 << 4;
  if (fpclassify(v) == FP_SUBNORMAL && v > 0)
    return 1 << 5;
  if (fpclassify(v) == FP_NORMAL && v > 0)
    return 1 << 6;
  if (isinf(v) && v > 0)
    return 1 << 7;
  if (isnan(v))
    return 1 << 9; // treat all NaN as quiet
  return 0;
}

static u64 fclass_d(double v) {
  if (isinf(v) && v < 0)
    return 1 << 0;
  if (fpclassify(v) == FP_NORMAL && v < 0)
    return 1 << 1;
  if (fpclassify(v) == FP_SUBNORMAL && v < 0)
    return 1 << 2;
  if (v == 0 && signbit(v))
    return 1 << 3;
  if (v == 0 && !signbit(v))
    return 1 << 4;
  if (fpclassify(v) == FP_SUBNORMAL && v > 0)
    return 1 << 5;
  if (fpclassify(v) == FP_NORMAL && v > 0)
    return 1 << 6;
  if (isinf(v) && v > 0)
    return 1 << 7;
  if (isnan(v))
    return 1 << 9;
  return 0;
}

// ---------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------
static Instruction decode(u32 raw) {
  Instruction inst = {.size = 4};

  inst.opcode = raw & 0x7F;
  inst.rd     = (raw >> 7) & 0x1F;
  inst.funct3 = (raw >> 12) & 0x07;
  inst.rs1    = (raw >> 15) & 0x1F;
  inst.rs2    = (raw >> 20) & 0x1F;
  inst.funct7 = (raw >> 25) & 0x7F;

  switch (inst.opcode) {
  case OP_JALR:
  case OP_LOAD:
  case OP_FLW_FLD:
  case OP_I_ARITH:
  case OP_I_ARITH_W:
  case OP_SYSTEM:
    inst.imm = sign_extend(raw >> 20, 12);
    break;

  case OP_STORE:
  case OP_FSW_FSD:
    inst.imm = sign_extend(((raw >> 25) << 5) | ((raw >> 7) & 0x1F), 12);
    break;

  case OP_BRANCH:
    inst.imm = sign_extend(((raw >> 31) << 12) | (((raw >> 7) & 1) << 11) |
                               (((raw >> 25) & 0x3F) << 5) | (((raw >> 8) & 0xF) << 1),
                           13);
    break;

  case OP_LUI:
  case OP_AUIPC:
    inst.imm = sign_extend(raw & 0xFFFFF000, 32);
    break;

  case OP_JAL:
    inst.imm = sign_extend(((raw >> 31) << 20) | (((raw >> 12) & 0xFF) << 12) |
                               (((raw >> 20) & 1) << 11) | (((raw >> 21) & 0x3FF) << 1),
                           21);
    break;

  default:
    break; // R-type, R4-type, OP_FP: no immediate
  }

  return inst;
}

// ---------------------------------------------------------------------------
// Float execute — single precision
// ---------------------------------------------------------------------------
static void execute_fp_single(CPU *cpu, Instruction inst, u64 pc) {
  u32   funct5 = inst.funct7 >> 2;
  u64   rs1i   = cpu->regs[inst.rs1]; // integer register value (for fcvt/fmv)
  float fs1    = freg_get_f(cpu, inst.rs1);
  float fs2    = freg_get_f(cpu, inst.rs2);

  switch (funct5) {
  case F5_FADD:
    freg_set_f(cpu, inst.rd, fs1 + fs2);
    break;
  case F5_FSUB:
    freg_set_f(cpu, inst.rd, fs1 - fs2);
    break;
  case F5_FMUL:
    freg_set_f(cpu, inst.rd, fs1 * fs2);
    break;
  case F5_FDIV:
    freg_set_f(cpu, inst.rd, fs1 / fs2);
    break;
  case F5_FSQRT:
    freg_set_f(cpu, inst.rd, sqrtf(fs1));
    break;

  case F5_FSGNJ: {
    u32 a, b, r;
    memcpy(&a, &cpu->fregs[inst.rs1], 4);
    memcpy(&b, &cpu->fregs[inst.rs2], 4);
    switch (inst.funct3) {
    case F3_FSGNJ:
      r = (a & 0x7FFFFFFF) | (b & 0x80000000);
      break;
    case F3_FSGNJN:
      r = (a & 0x7FFFFFFF) | (~b & 0x80000000);
      break;
    case F3_FSGNJX:
      r = (a & 0x7FFFFFFF) | ((a ^ b) & 0x80000000);
      break;
    default:
      fprintf(stderr, "unknown fsgnj funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    memcpy(&cpu->fregs[inst.rd], &r, 4);
    break;
  }

  case F5_FMINMAX:
    if (inst.funct3 == F3_FMIN)
      freg_set_f(cpu, inst.rd, isnan(fs1) ? fs2 : isnan(fs2) ? fs1 : fminf(fs1, fs2));
    else
      freg_set_f(cpu, inst.rd, isnan(fs1) ? fs2 : isnan(fs2) ? fs1 : fmaxf(fs1, fs2));
    break;

  case F5_FCVT_FF: // fcvt.s.d — double to single
    freg_set_f(cpu, inst.rd, (float)freg_get_d(cpu, inst.rs1));
    break;

  case F5_FCVT_FI: // float -> int
    switch (inst.rs2) {
    case 0:
      reg_write(cpu, inst.rd, (u64)(i64)(i32)fs1);
      break; // fcvt.w.s
    case 1:
      reg_write(cpu, inst.rd, (u64)(u32)fs1);
      break; // fcvt.wu.s
    case 2:
      reg_write(cpu, inst.rd, (u64)(i64)fs1);
      break; // fcvt.l.s
    case 3:
      reg_write(cpu, inst.rd, (u64)fs1);
      break; // fcvt.lu.s
    default:
      fprintf(stderr, "unknown fcvt.fi rs2=0x%x at pc=0x%llx\n", inst.rs2, pc);
      exit(1);
    }
    break;

  case F5_FCVT_IF: // int -> float
    switch (inst.rs2) {
    case 0:
      freg_set_f(cpu, inst.rd, (float)(i32)rs1i);
      break; // fcvt.s.w
    case 1:
      freg_set_f(cpu, inst.rd, (float)(u32)rs1i);
      break; // fcvt.s.wu
    case 2:
      freg_set_f(cpu, inst.rd, (float)(i64)rs1i);
      break; // fcvt.s.l
    case 3:
      freg_set_f(cpu, inst.rd, (float)rs1i);
      break; // fcvt.s.lu
    default:
      fprintf(stderr, "unknown fcvt.if rs2=0x%x at pc=0x%llx\n", inst.rs2, pc);
      exit(1);
    }
    break;

  case F5_FMV_XI: // fmv.x.w or fclass.s
    if (inst.funct3 == 0) {
      u32 bits;
      memcpy(&bits, &cpu->fregs[inst.rs1], 4);
      reg_write(cpu, inst.rd, (u64)(i64)(i32)bits);
    } else {
      reg_write(cpu, inst.rd, fclass_f(fs1));
    }
    break;

  case F5_FCMP:
    switch (inst.funct3) {
    case F3_FEQ:
      reg_write(cpu, inst.rd, fs1 == fs2 ? 1 : 0);
      break;
    case F3_FLT:
      reg_write(cpu, inst.rd, fs1 < fs2 ? 1 : 0);
      break;
    case F3_FLE:
      reg_write(cpu, inst.rd, fs1 <= fs2 ? 1 : 0);
      break;
    default:
      fprintf(stderr, "unknown fcmp funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    break;

  case F5_FMV_IX: { // fmv.w.x — int reg -> float reg (raw bits)
    u32 bits = (u32)rs1i;
    memcpy(&cpu->fregs[inst.rd], &bits, 4);
    break;
  }

  default:
    fprintf(stderr, "unknown FP funct5=0x%x (single) at pc=0x%llx\n", funct5, pc);
    exit(1);
  }
}

// ---------------------------------------------------------------------------
// Float execute — double precision
// ---------------------------------------------------------------------------
static void execute_fp_double(CPU *cpu, Instruction inst, u64 pc) {
  u32    funct5 = inst.funct7 >> 2;
  u64    rs1i   = cpu->regs[inst.rs1];
  double fd1    = freg_get_d(cpu, inst.rs1);
  double fd2    = freg_get_d(cpu, inst.rs2);

  switch (funct5) {
  case F5_FADD:
    freg_set_d(cpu, inst.rd, fd1 + fd2);
    break;
  case F5_FSUB:
    freg_set_d(cpu, inst.rd, fd1 - fd2);
    break;
  case F5_FMUL:
    freg_set_d(cpu, inst.rd, fd1 * fd2);
    break;
  case F5_FDIV:
    freg_set_d(cpu, inst.rd, fd1 / fd2);
    break;
  case F5_FSQRT:
    freg_set_d(cpu, inst.rd, sqrt(fd1));
    break;

  case F5_FSGNJ: {
    u64 a         = cpu->fregs[inst.rs1];
    u64 b         = cpu->fregs[inst.rs2];
    u64 sign_mask = 0x8000000000000000ULL;
    switch (inst.funct3) {
    case F3_FSGNJ:
      cpu->fregs[inst.rd] = (a & ~sign_mask) | (b & sign_mask);
      break;
    case F3_FSGNJN:
      cpu->fregs[inst.rd] = (a & ~sign_mask) | (~b & sign_mask);
      break;
    case F3_FSGNJX:
      cpu->fregs[inst.rd] = (a & ~sign_mask) | ((a ^ b) & sign_mask);
      break;
    default:
      fprintf(stderr, "unknown fsgnj funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    break;
  }

  case F5_FMINMAX:
    if (inst.funct3 == F3_FMIN)
      freg_set_d(cpu, inst.rd, isnan(fd1) ? fd2 : isnan(fd2) ? fd1 : fmin(fd1, fd2));
    else
      freg_set_d(cpu, inst.rd, isnan(fd1) ? fd2 : isnan(fd2) ? fd1 : fmax(fd1, fd2));
    break;

  case F5_FCVT_FF: // fcvt.d.s — single to double
    freg_set_d(cpu, inst.rd, (double)freg_get_f(cpu, inst.rs1));
    break;

  case F5_FCVT_FI: // double -> int
    switch (inst.rs2) {
    case 0:
      reg_write(cpu, inst.rd, (u64)(i64)(i32)fd1);
      break; // fcvt.w.d
    case 1:
      reg_write(cpu, inst.rd, (u64)(u32)fd1);
      break; // fcvt.wu.d
    case 2:
      reg_write(cpu, inst.rd, (u64)(i64)fd1);
      break; // fcvt.l.d
    case 3:
      reg_write(cpu, inst.rd, (u64)fd1);
      break; // fcvt.lu.d
    default:
      fprintf(stderr, "unknown fcvt.fi rs2=0x%x at pc=0x%llx\n", inst.rs2, pc);
      exit(1);
    }
    break;

  case F5_FCVT_IF: // int -> double
    switch (inst.rs2) {
    case 0:
      freg_set_d(cpu, inst.rd, (double)(i32)rs1i);
      break; // fcvt.d.w
    case 1:
      freg_set_d(cpu, inst.rd, (double)(u32)rs1i);
      break; // fcvt.d.wu
    case 2:
      freg_set_d(cpu, inst.rd, (double)(i64)rs1i);
      break; // fcvt.d.l
    case 3:
      freg_set_d(cpu, inst.rd, (double)rs1i);
      break; // fcvt.d.lu
    default:
      fprintf(stderr, "unknown fcvt.if rs2=0x%x at pc=0x%llx\n", inst.rs2, pc);
      exit(1);
    }
    break;

  case F5_FMV_XI: // fmv.x.d or fclass.d
    if (inst.funct3 == 0)
      reg_write(cpu, inst.rd, cpu->fregs[inst.rs1]);
    else
      reg_write(cpu, inst.rd, fclass_d(fd1));
    break;

  case F5_FCMP:
    switch (inst.funct3) {
    case F3_FEQ:
      reg_write(cpu, inst.rd, fd1 == fd2 ? 1 : 0);
      break;
    case F3_FLT:
      reg_write(cpu, inst.rd, fd1 < fd2 ? 1 : 0);
      break;
    case F3_FLE:
      reg_write(cpu, inst.rd, fd1 <= fd2 ? 1 : 0);
      break;
    default:
      fprintf(stderr, "unknown fcmp funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    break;

  case F5_FMV_IX: // fmv.d.x — int reg -> float reg (raw bits)
    cpu->fregs[inst.rd] = rs1i;
    break;

  default:
    fprintf(stderr, "unknown FP funct5=0x%x (double) at pc=0x%llx\n", funct5, pc);
    exit(1);
  }
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------
static void execute(CPU *cpu, const Memory *mem, Instruction inst) {
  u64 pc  = cpu->pc;
  u64 rs1 = cpu->regs[inst.rs1];
  u64 rs2 = cpu->regs[inst.rs2];
  i64 imm = inst.imm;

  switch (inst.opcode) {

  case OP_LUI:
    reg_write(cpu, inst.rd, (u64)imm);
    break;

  case OP_AUIPC:
    reg_write(cpu, inst.rd, pc + (u64)imm);
    break;

  case OP_JAL:
    reg_write(cpu, inst.rd, pc + inst.size);
    cpu->pc = pc + (u64)imm;
    return;

  case OP_JALR:
    reg_write(cpu, inst.rd, pc + inst.size);
    cpu->pc = (rs1 + (u64)imm) & ~(u64)1;
    return;

  case OP_BRANCH: {
    int taken = 0;
    switch (inst.funct3) {
    case F3_BEQ:
      taken = rs1 == rs2;
      break;
    case F3_BNE:
      taken = rs1 != rs2;
      break;
    case F3_BLT:
      taken = (i64)rs1 < (i64)rs2;
      break;
    case F3_BGE:
      taken = (i64)rs1 >= (i64)rs2;
      break;
    case F3_BLTU:
      taken = rs1 < rs2;
      break;
    case F3_BGEU:
      taken = rs1 >= rs2;
      break;
    default:
      fprintf(stderr, "unknown branch funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    cpu->pc = taken ? pc + (u64)imm : pc + inst.size;
    return;
  }

  case OP_LOAD: {
    u64 va = rs1 + (u64)imm;
    u64 val = 0;
    switch (inst.funct3) {
    case F3_LB: {
      u8 tmp;
      if (!vm_read8(cpu, mem, va, &tmp))
        return;
      val = (u64)(i64)(i8)tmp;
      break;
    }
    case F3_LH: {
      u16 tmp;
      if (!vm_read16(cpu, mem, va, &tmp))
        return;
      val = (u64)(i64)(i16)tmp;
      break;
    }
    case F3_LW: {
      u32 tmp;
      if (!vm_read32(cpu, mem, va, &tmp))
        return;
      val = (u64)(i64)(i32)tmp;
      break;
    }
    case F3_LD:
      if (!vm_read64(cpu, mem, va, &val))
        return;
      break;
    case F3_LBU: {
      u8 tmp;
      if (!vm_read8(cpu, mem, va, &tmp))
        return;
      val = (u64)tmp;
      break;
    }
    case F3_LHU: {
      u16 tmp;
      if (!vm_read16(cpu, mem, va, &tmp))
        return;
      val = (u64)tmp;
      break;
    }
    case F3_LWU: {
      u32 tmp;
      if (!vm_read32(cpu, mem, va, &tmp))
        return;
      val = (u64)tmp;
      break;
    }
    default:
      fprintf(stderr, "unknown load funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    reg_write(cpu, inst.rd, val);
    break;
  }

  case OP_STORE: {
    u64 va = rs1 + (u64)imm;
    bool ok;
    switch (inst.funct3) {
    case F3_SB:
      ok = vm_write8(cpu, mem, va, (u8)rs2);
      break;
    case F3_SH:
      ok = vm_write16(cpu, mem, va, (u16)rs2);
      break;
    case F3_SW:
      ok = vm_write32(cpu, mem, va, (u32)rs2);
      break;
    case F3_SD:
      ok = vm_write64(cpu, mem, va, rs2);
      break;
    default:
      fprintf(stderr, "unknown store funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    if (!ok)
      return;
    break;
  }

  case OP_I_ARITH: {
    u64 val   = 0;
    u32 shamt = (u32)imm & 0x3F;
    switch (inst.funct3) {
    case F3_ADD_SUB:
      val = rs1 + (u64)imm;
      break; // ADDI
    case F3_SLT:
      val = (i64)rs1 < imm ? 1 : 0;
      break; // SLTI
    case F3_SLTU:
      val = rs1 < (u64)imm ? 1 : 0;
      break; // SLTIU
    case F3_XOR:
      val = rs1 ^ (u64)imm;
      break; // XORI
    case F3_OR:
      val = rs1 | (u64)imm;
      break; // ORI
    case F3_AND:
      val = rs1 & (u64)imm;
      break; // ANDI
    case F3_SLL:
      val = rs1 << shamt;
      break; // SLLI
    case F3_SRL_SRA:
      val = (inst.funct7 & F7_ALT) ? (u64)((i64)rs1 >> shamt) // SRAI
                                   : rs1 >> shamt;            // SRLI
      break;
    default:
      fprintf(stderr, "unknown I-arith funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    reg_write(cpu, inst.rd, val);
    break;
  }

  case OP_R_ARITH: {
    u64 val   = 0;
    u32 shamt = (u32)rs2 & 0x3F;
    if (inst.funct7 == F7_MEXT) { // M extension — multiply/divide
      switch (inst.funct3) {
      case F3_MUL:
        val = rs1 * rs2;
        break;
      case F3_MULH:
        val = (u64)(((i128)rs1 * (i128)rs2) >> 64);
        break;
      case F3_MULHSU:
        val = (u64)(((i128)rs1 * (u128)rs2) >> 64);
        break;
      case F3_MULHU:
        val = (u64)(((u128)rs1 * (u128)rs2) >> 64);
        break;
      case F3_DIV:
        val = rs2 == 0 ? (u64)-1 : (u64)((i64)rs1 / (i64)rs2);
        break;
      case F3_DIVU:
        val = rs2 == 0 ? (u64)-1 : rs1 / rs2;
        break;
      case F3_REM:
        val = rs2 == 0 ? rs1 : (u64)((i64)rs1 % (i64)rs2);
        break;
      case F3_REMU:
        val = rs2 == 0 ? rs1 : rs1 % rs2;
        break;
      default:
        fprintf(stderr, "unknown M-ext funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
        exit(1);
      }
    } else {
      switch (inst.funct3) {
      case F3_ADD_SUB:
        val = inst.funct7 == F7_ALT ? rs1 - rs2 : rs1 + rs2; // ADD/SUB
        break;
      case F3_SLL:
        val = rs1 << shamt;
        break; // SLL
      case F3_SLT:
        val = (i64)rs1 < (i64)rs2 ? 1 : 0;
        break; // SLT
      case F3_SLTU:
        val = rs1 < rs2 ? 1 : 0;
        break; // SLTU
      case F3_XOR:
        val = rs1 ^ rs2;
        break; // XOR
      case F3_SRL_SRA:
        val = inst.funct7 == F7_ALT ? (u64)((i64)rs1 >> shamt) // SRA
                                    : rs1 >> shamt;            // SRL
        break;
      case F3_OR:
        val = rs1 | rs2;
        break; // OR
      case F3_AND:
        val = rs1 & rs2;
        break; // AND
      default:
        fprintf(stderr, "unknown R-arith funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
        exit(1);
      }
    }
    reg_write(cpu, inst.rd, val);
    break;
  }

  case OP_I_ARITH_W: {
    u32 rs1w  = (u32)rs1;
    u32 shamt = (u32)imm & 0x1F;
    u32 val32 = 0;
    switch (inst.funct3) {
    case F3_ADD_SUB:
      val32 = rs1w + (u32)imm;
      break; // ADDIW
    case F3_SLL:
      val32 = rs1w << shamt;
      break; // SLLIW
    case F3_SRL_SRA:
      val32 = inst.funct7 == F7_ALT ? (u32)((i32)rs1w >> shamt) // SRAIW
                                    : rs1w >> shamt;            // SRLIW
      break;
    default:
      fprintf(stderr, "unknown I-arith-W funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
      exit(1);
    }
    reg_write(cpu, inst.rd, (u64)(i64)(i32)val32);
    break;
  }

  case OP_R_ARITH_W: {
    u32 rs1w  = (u32)rs1;
    u32 rs2w  = (u32)rs2;
    u32 shamt = rs2w & 0x1F;
    u32 val32 = 0;
    if (inst.funct7 == F7_MEXT) { // M extension — 32-bit multiply/divide
      switch (inst.funct3) {
      case F3_MUL:
        val32 = rs1w * rs2w;
        break; // MULW
      case F3_DIV:
        val32 = rs2w == 0 ? (u32)-1 : (u32)((i32)rs1w / (i32)rs2w);
        break; // DIVW
      case F3_DIVU:
        val32 = rs2w == 0 ? (u32)-1 : rs1w / rs2w;
        break; // DIVUW
      case F3_REM:
        val32 = rs2w == 0 ? rs1w : (u32)((i32)rs1w % (i32)rs2w);
        break; // REMW
      case F3_REMU:
        val32 = rs2w == 0 ? rs1w : rs1w % rs2w;
        break; // REMUW
      default:
        fprintf(stderr, "unknown M-ext-W funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
        exit(1);
      }
    } else {
      switch (inst.funct3) {
      case F3_ADD_SUB:
        val32 = inst.funct7 == F7_ALT ? rs1w - rs2w : rs1w + rs2w; // ADDW/SUBW
        break;
      case F3_SLL:
        val32 = rs1w << shamt;
        break; // SLLW
      case F3_SRL_SRA:
        val32 = inst.funct7 == F7_ALT ? (u32)((i32)rs1w >> shamt) // SRAW
                                      : rs1w >> shamt;            // SRLW
        break;
      default:
        fprintf(stderr, "unknown R-arith-W funct3=0x%x at pc=0x%llx\n", inst.funct3, pc);
        exit(1);
      }
    }
    reg_write(cpu, inst.rd, (u64)(i64)(i32)val32);
    break;
  }

  // -------------------------------------------------------------------------
  // Float loads
  // -------------------------------------------------------------------------
  case OP_FLW_FLD: {
    u64 va = rs1 + (u64)imm;
    if (inst.funct3 == F3_FLW_FSW) {
      u32 bits;
      if (!vm_read32(cpu, mem, va, &bits))
        return;
      memcpy(&cpu->fregs[inst.rd], &bits, 4);
    } else { // FLD
      u64 bits;
      if (!vm_read64(cpu, mem, va, &bits))
        return;
      cpu->fregs[inst.rd] = bits;
    }
    break;
  }

  // -------------------------------------------------------------------------
  // Float stores
  // -------------------------------------------------------------------------
  case OP_FSW_FSD: {
    u64 va = rs1 + (u64)imm;
    if (inst.funct3 == F3_FLW_FSW) {
      u32 bits;
      memcpy(&bits, &cpu->fregs[inst.rs2], 4);
      if (!vm_write32(cpu, mem, va, bits))
        return;
    } else { // FSD
      if (!vm_write64(cpu, mem, va, cpu->fregs[inst.rs2]))
        return;
    }
    break;
  }

  // -------------------------------------------------------------------------
  // Fused multiply-add (R4-type)
  // rs3 = funct7 >> 2, fmt = funct7 & 0x3
  // -------------------------------------------------------------------------
  case OP_FMADD:
  case OP_FMSUB:
  case OP_FNMSUB:
  case OP_FNMADD: {
    u32 rs3 = inst.funct7 >> 2;
    u32 fmt = inst.funct7 & 0x3;
    if (fmt == FMT_SINGLE) {
      float a = freg_get_f(cpu, inst.rs1);
      float b = freg_get_f(cpu, inst.rs2);
      float c = freg_get_f(cpu, rs3);
      float r;
      switch (inst.opcode) {
      case OP_FMADD:
        r = a * b + c;
        break;
      case OP_FMSUB:
        r = a * b - c;
        break;
      case OP_FNMSUB:
        r = -a * b + c;
        break;
      case OP_FNMADD:
        r = -a * b - c;
        break;
      default:
        r = 0;
      }
      freg_set_f(cpu, inst.rd, r);
    } else {
      double a = freg_get_d(cpu, inst.rs1);
      double b = freg_get_d(cpu, inst.rs2);
      double c = freg_get_d(cpu, rs3);
      double r;
      switch (inst.opcode) {
      case OP_FMADD:
        r = a * b + c;
        break;
      case OP_FMSUB:
        r = a * b - c;
        break;
      case OP_FNMSUB:
        r = -a * b + c;
        break;
      case OP_FNMADD:
        r = -a * b - c;
        break;
      default:
        r = 0;
      }
      freg_set_d(cpu, inst.rd, r);
    }
    break;
  }

  // -------------------------------------------------------------------------
  // All other float ops — dispatch to single or double handler
  // -------------------------------------------------------------------------
  case OP_FENCE: // FENCE / FENCE.I — NOP (no caches or write buffers to flush)
    break;

  case OP_AMO:
    execute_a(cpu, mem, inst);
    break;

  case OP_FP: {
    u32 fmt = inst.funct7 & 0x3;
    if (fmt == FMT_SINGLE)
      execute_fp_single(cpu, inst, pc);
    else
      execute_fp_double(cpu, inst, pc);
    break;
  }

  case OP_SYSTEM:
    if (inst.funct3 == 0) {
      execute_trap_return(cpu, mem, imm);
      return; // pc already updated
    } else {
      // CSR instructions — funct3 encodes the operation
      u32 csr_addr = (u32)(inst.imm & 0xFFF);
      u32 min_priv = (csr_addr >> 8) & 0x3; // bits[9:8] encode minimum privilege level
      if (cpu->privilege < min_priv) {
        cpu->pc = pc;
        cpu_trap(cpu, EXC_CAUSE_ILLEGAL_INSTR, 0);
        return;
      }
      u64 old_val = csr_read(cpu, csr_addr);
      u64 src     = (inst.funct3 & 0x4) ? inst.rs1 : cpu->regs[inst.rs1]; // immediate vs register
      u64 new_val;

      switch (inst.funct3 & 0x3) {
      case 0x1:
        new_val = src;
        break; // csrrw / csrrwi
      case 0x2:
        new_val = old_val | src;
        break; // csrrs / csrrsi
      case 0x3:
        new_val = old_val & ~src;
        break; // csrrc / csrrci
      default:
        new_val = old_val;
        break;
      }

      // for csrrs/csrrc, skip write if src is zero (no side effects)
      if ((inst.funct3 & 0x3) == 0x1 || src != 0)
        csr_write(cpu, csr_addr, new_val);

      reg_write(cpu, inst.rd, old_val);
    }
    break;

  default:
    fprintf(stderr, "unknown opcode 0x%02x at pc=0x%llx\n", inst.opcode, pc);
    exit(1);
  }

  cpu->pc += inst.size;
}

// ---------------------------------------------------------------------------
// Trap-return and environment instructions (funct3 == 0 in OP_SYSTEM)
// ---------------------------------------------------------------------------
static void execute_trap_return(CPU *cpu, const Memory *mem, i64 imm) {
  switch (imm) {
  case SYSTEM_ECALL:
    if (cpu->privilege == PRIV_S)
      sbi_ecall(cpu, mem);
    else
      // cause = 8 + privilege level (spec-defined)
      cpu_trap(cpu, 8 + cpu->privilege, 0);
    break;

  case SYSTEM_EBREAK:
    cpu_trap(cpu, EXC_CAUSE_BREAKPOINT, 0);
    break;

  case SYSTEM_WFI: // interrupt fires on next cpu_step — no sleep needed
    cpu->pc += INSN_SIZE;
    break;
  case SYSTEM_SRET: {
    if (cpu->privilege < PRIV_S) {
      cpu_trap(cpu, EXC_CAUSE_ILLEGAL_INSTR, 0);
      break;
    }
    u64 status = csr_read(cpu, CSR_MSTATUS);

    // restore privilege from SPP, then clear SPP
    cpu->privilege = (status & MSTATUS_SPP) ? PRIV_S : PRIV_U;
    status &= ~MSTATUS_SPP;

    // restore SIE from SPIE, then set SPIE
    status &= ~MSTATUS_SIE;
    if (status & MSTATUS_SPIE)
      status |= MSTATUS_SIE;
    status |= MSTATUS_SPIE;

    csr_write(cpu, CSR_MSTATUS, status);
    cpu->pc = csr_read(cpu, CSR_SEPC);
    break;
  }
  case 0x302: { // mret — requires M-mode
    if (cpu->privilege < PRIV_M) {
      cpu_trap(cpu, EXC_CAUSE_ILLEGAL_INSTR, 0);
      break;
    }
    u64 status = csr_read(cpu, CSR_MSTATUS);

    // restore privilege from MPP, then clear MPP to U(0)
    cpu->privilege = (status & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
    status &= ~MSTATUS_MPP_MASK;

    // restore MIE from MPIE, then set MPIE
    status &= ~MSTATUS_MIE;
    if (status & MSTATUS_MPIE)
      status |= MSTATUS_MIE;
    status |= MSTATUS_MPIE;

    csr_write(cpu, CSR_MSTATUS, status);
    cpu->pc = csr_read(cpu, CSR_MEPC);
    break;
  }
  default: {
    u32 funct7 = (u32)imm >> 5;
    if (funct7 == SYSTEM_FUNCT7_SFENCE_VMA) { // no TLB to flush
      cpu->pc += INSN_SIZE;
      break;
    }
    fprintf(stderr, "unknown SYSTEM imm=0x%llx at pc=0x%llx\n", (u64)imm, cpu->pc);
    exit(1);
  }
  }
}

// ---------------------------------------------------------------------------
// Trap handling
// ---------------------------------------------------------------------------
void cpu_trap(CPU *cpu, u64 cause, u64 tval) {
  // Bit 63 distinguishes interrupts from exceptions; each uses its own delegation CSR
  bool is_interrupt = (cause >> 63) != 0;

  u32 deleg_csr = is_interrupt ? CSR_MIDELEG : CSR_MEDELEG;
  u64 cause_idx = cause & 0x3F; // strip interrupt bit for delegation lookup

  bool to_s = (cpu->privilege <= PRIV_S) && (csr_read(cpu, deleg_csr) & ((u64)1 << cause_idx));

  if (to_s) {
    csr_write(cpu, CSR_SEPC, cpu->pc);
    csr_write(cpu, CSR_SCAUSE, cause);
    csr_write(cpu, CSR_STVAL, tval);

    u64 status = csr_read(cpu, CSR_MSTATUS);

    // save SIE into SPIE
    status &= ~MSTATUS_SPIE;
    if (status & MSTATUS_SIE)
      status |= MSTATUS_SPIE;

    // clear SIE
    status &= ~MSTATUS_SIE;

    // save current privilege into SPP (1 bit: 0=U, 1=S)
    status &= ~MSTATUS_SPP;
    if (cpu->privilege == PRIV_S)
      status |= MSTATUS_SPP;

    csr_write(cpu, CSR_MSTATUS, status);
    cpu->privilege = PRIV_S;
    cpu->pc        = csr_read(cpu, CSR_STVEC) & ~(u64)0x3;
  } else {
    csr_write(cpu, CSR_MEPC, cpu->pc);
    csr_write(cpu, CSR_MCAUSE, cause);
    csr_write(cpu, CSR_MTVAL, tval);

    u64 status = csr_read(cpu, CSR_MSTATUS);

    // save MIE into MPIE
    status &= ~MSTATUS_MPIE;
    if (status & MSTATUS_MIE)
      status |= MSTATUS_MPIE;

    // clear MIE
    status &= ~MSTATUS_MIE;

    // save current privilege into MPP (2-bit field at MSTATUS_MPP_SHIFT)
    status &= ~((u64)0x3 << MSTATUS_MPP_SHIFT);
    status |= (u64)cpu->privilege << MSTATUS_MPP_SHIFT;

    csr_write(cpu, CSR_MSTATUS, status);
    cpu->privilege = PRIV_M;
    cpu->pc        = csr_read(cpu, CSR_MTVEC) & ~(u64)0x3;
  }
}

// ---------------------------------------------------------------------------
// Interrupt sources and delivery
// ---------------------------------------------------------------------------
#define MAX_IRQ_SOURCES 8
static IrqSourceFn s_irq_sources[MAX_IRQ_SOURCES];
static int         s_irq_source_count = 0;

void cpu_add_irq_source(IrqSourceFn fn) {
  if (s_irq_source_count < MAX_IRQ_SOURCES)
    s_irq_sources[s_irq_source_count++] = fn;
}

void cpu_raise_irq(CPU *cpu, u64 bit) { cpu->csrs[CSR_MIP] |= bit; }
void cpu_lower_irq(CPU *cpu, u64 bit) { cpu->csrs[CSR_MIP] &= ~bit; }

// Poll all registered IRQ sources, then deliver the highest-priority pending interrupt.
// Returns true if a trap was taken (caller should skip instruction fetch).
static bool cpu_check_interrupts(CPU *cpu) {
  for (int i = 0; i < s_irq_source_count; i++)
    s_irq_sources[i](cpu);

  u64 pending = cpu->csrs[CSR_MIP] & cpu->csrs[CSR_MIE];
  if (!pending)
    return false;

  u64 status = cpu->csrs[CSR_MSTATUS];

  // Non-delegated: deliver in M-mode (fires if MIE set, or we're below M-mode)
  u64 m_pending = pending & ~cpu->csrs[CSR_MIDELEG];
  if (m_pending && (cpu->privilege < PRIV_M || (status & MSTATUS_MIE))) {
    for (int bit = 11; bit >= 0; bit--) {
      if (m_pending & ((u64)1 << bit)) {
        cpu_trap(cpu, ((u64)1 << 63) | (u64)bit, 0);
        return true;
      }
    }
  }

  // Delegated: deliver in S-mode (fires if SIE set, or we're in U-mode)
  u64 s_pending = pending & cpu->csrs[CSR_MIDELEG];
  if (s_pending &&
      (cpu->privilege == PRIV_U || (cpu->privilege == PRIV_S && (status & MSTATUS_SIE)))) {
    for (int bit = 9; bit >= 0; bit--) {
      if (s_pending & ((u64)1 << bit)) {
        cpu_trap(cpu, ((u64)1 << 63) | (u64)bit, 0);
        return true;
      }
    }
  }

  return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void cpu_init(CPU *cpu) {
  // Real hardware resets in M-mode. We start there too, then delegate all
  // standard exceptions and interrupts to S-mode so Linux (running in S-mode)
  // can handle its own traps without going through firmware on every ecall/fault.
  cpu->privilege         = PRIV_M;
  cpu->csrs[CSR_MEDELEG] = 0xFFFF; // delegate all exceptions to S-mode
  cpu->csrs[CSR_MIDELEG] = 0xFFFF; // delegate all interrupts to S-mode

  cpu->pc       = 0x80000000;
  cpu->regs[10] = 0; // a0 = hart ID
  cpu->regs[11] = 0; // a1 = DTB address (none yet)
}

void cpu_step(CPU *cpu, const Memory *mem) {
  cpu->steps++; // advance the unified cycle/time/instret counter
  if (cpu_check_interrupts(cpu))
    return;
  u64 pc_pa = mmu_translate(cpu, mem, cpu->pc, MMU_FETCH);
  if (pc_pa == MMU_FAULT)
    return;
  u16         lo   = mem_read16(mem, pc_pa);
  Instruction inst = ((lo & 0x3) != 0x3) ? decode_rvc(lo) : decode(mem_read32(mem, pc_pa));
  execute(cpu, mem, inst);
}
