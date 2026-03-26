#include "exec_c.h"

#include "opcodes.h"

// ---------------------------------------------------------------------------
// RVC field positions and masks
// ---------------------------------------------------------------------------

// Common
#define RVC_OP_SHIFT 0
#define RVC_OP_MASK 0x3
#define RVC_F3_SHIFT 13
#define RVC_F3_MASK 0x7
#define RVC_B12_SHIFT 12
#define RVC_B12_MASK 0x1

// Compressed (3-bit) register fields → x8–x15 via cl()
#define RVC_CL_RDS_SHIFT 2  // CL/CS rd'/rs2': bits[4:2]
#define RVC_CL_RS1S_SHIFT 7 // CL/CB rs1':     bits[9:7]
#define RVC_CL_REG_MASK 0x7

// Full (5-bit) register fields
#define RVC_CI_RD_SHIFT 7  // CI/CR rd:   bits[11:7]
#define RVC_CR_RS2_SHIFT 2 // CR/CSS rs2: bits[6:2]
#define RVC_REG_MASK 0x1F

// Q1 funct3=4 sub-op selectors
#define RVC_Q1_SUB_SHIFT 10 // bits[11:10] — selects SRLI/SRAI/ANDI/arith
#define RVC_Q1_SUB_MASK 0x3
#define RVC_Q1_ARITH_SHIFT 5 // bits[6:5] — selects SUB/XOR/OR/AND/SUBW/ADDW
#define RVC_Q1_ARITH_MASK 0x3

// ---------------------------------------------------------------------------
// Quadrant / funct3 named values
// ---------------------------------------------------------------------------

#define RVC_Q0 0x0
#define RVC_Q1 0x1
#define RVC_Q2 0x2

// Quadrant 0 funct3
#define RVC_Q0_ADDI4SPN 0x0
#define RVC_Q0_FLD 0x1
#define RVC_Q0_LW 0x2
#define RVC_Q0_LD 0x3
// 0x4 reserved
#define RVC_Q0_FSD 0x5
#define RVC_Q0_SW 0x6
#define RVC_Q0_SD 0x7

// Quadrant 1 funct3
#define RVC_Q1_ADDI 0x0  // also C.NOP when rd=0 and imm=0
#define RVC_Q1_ADDIW 0x1 // RV64 only
#define RVC_Q1_LI 0x2
#define RVC_Q1_LUI 0x3      // also C.ADDI16SP when rd=x2
#define RVC_Q1_MISC_ALU 0x4 // SRLI / SRAI / ANDI / SUB / XOR / OR / AND / SUBW / ADDW
#define RVC_Q1_J 0x5
#define RVC_Q1_BEQZ 0x6
#define RVC_Q1_BNEZ 0x7

// Quadrant 1 MISC_ALU sub (bits[11:10])
#define RVC_ALU_SRLI 0x0
#define RVC_ALU_SRAI 0x1
#define RVC_ALU_ANDI 0x2
#define RVC_ALU_ARITH 0x3

// Quadrant 1 MISC_ALU arithmetic op (bits[6:5])
#define RVC_ARITH_SUB 0x0
#define RVC_ARITH_XOR 0x1
#define RVC_ARITH_OR 0x2
#define RVC_ARITH_AND 0x3

// Quadrant 2 funct3
#define RVC_Q2_SLLI 0x0
#define RVC_Q2_FLDSP 0x1
#define RVC_Q2_LWSP 0x2
#define RVC_Q2_LDSP 0x3
#define RVC_Q2_CR 0x4 // JR / MV / EBREAK / JALR / ADD
#define RVC_Q2_FSDSP 0x5
#define RVC_Q2_SWSP 0x6
#define RVC_Q2_SDSP 0x7

// ---------------------------------------------------------------------------
// Register field helpers
// ---------------------------------------------------------------------------

// 3-bit CL register field → x8–x15
static u32 cl(u32 r) { return r + 8; }

static u32 rvc_rd_s(u16 r) { return cl((r >> RVC_CL_RDS_SHIFT) & RVC_CL_REG_MASK); }
static u32 rvc_rs1_s(u16 r) { return cl((r >> RVC_CL_RS1S_SHIFT) & RVC_CL_REG_MASK); }
static u32 rvc_rd(u16 r) { return (r >> RVC_CI_RD_SHIFT) & RVC_REG_MASK; }
static u32 rvc_rs2(u16 r) { return (r >> RVC_CR_RS2_SHIFT) & RVC_REG_MASK; }

// ---------------------------------------------------------------------------
// Sub-op selector helpers
// ---------------------------------------------------------------------------

static u32 rvc_b12(u16 r) { return (r >> RVC_B12_SHIFT) & RVC_B12_MASK; }
static u32 rvc_sub(u16 r) { return (r >> RVC_Q1_SUB_SHIFT) & RVC_Q1_SUB_MASK; }
static u32 rvc_arith(u16 r) { return (r >> RVC_Q1_ARITH_SHIFT) & RVC_Q1_ARITH_MASK; }

// ---------------------------------------------------------------------------
// Sign-extend helper
// ---------------------------------------------------------------------------
static i64 sext(u64 val, int bits) {
  int shift = 64 - bits;
  return (i64)(val << shift) >> shift;
}

// ---------------------------------------------------------------------------
// Immediate decoders — one per encoding format
// ---------------------------------------------------------------------------

// CI: imm[5]=inst[12], imm[4:0]=inst[6:2]
static i64 imm_ci(u16 r) { return sext(((u64)(r >> 12) & 1) << 5 | ((r >> 2) & 0x1F), 6); }

// CI special — C.ADDI16SP
static i64 imm_addi16sp(u16 r) {
  u64 v = ((u64)(r >> 12) & 1) << 9 | ((r >> 6) & 1) << 4 | ((r >> 5) & 1) << 6 |
          ((r >> 3) & 3) << 7 | ((r >> 2) & 1) << 5;
  return sext(v, 10);
}

// CI special — C.LUI: nzimm[17:12]=inst[12|6:2]
static i64 imm_lui(u16 r) {
  u64 v = ((u64)(r >> 12) & 1) << 17 | ((r >> 2) & 0x1F) << 12;
  return sext(v, 18);
}

// CL/CS word: offset[5:3]=inst[12:10], offset[2]=inst[6], offset[6]=inst[5]
static i64 imm_clw(u16 r) {
  return (i64)(((r >> 10) & 7) << 3 | ((r >> 6) & 1) << 2 | ((r >> 5) & 1) << 6);
}

// CL/CS double: offset[5:3]=inst[12:10], offset[7:6]=inst[6:5]
static i64 imm_cld(u16 r) { return (i64)(((r >> 10) & 7) << 3 | ((r >> 5) & 3) << 6); }

// CJ: 11-bit offset, heavily scrambled
static i64 imm_cj(u16 r) {
  u64 v = ((u64)(r >> 12) & 1) << 11 | ((r >> 11) & 1) << 4 | ((r >> 9) & 3) << 8 |
          ((r >> 8) & 1) << 10 | ((r >> 7) & 1) << 6 | ((r >> 6) & 1) << 7 | ((r >> 3) & 7) << 1 |
          ((r >> 2) & 1) << 5;
  return sext(v, 12);
}

// CB: 8-bit branch offset
static i64 imm_cb(u16 r) {
  u64 v = ((u64)(r >> 12) & 1) << 8 | ((r >> 10) & 3) << 3 | ((r >> 5) & 3) << 6 |
          ((r >> 3) & 3) << 1 | ((r >> 2) & 1) << 5;
  return sext(v, 9);
}

// CISP word — C.LWSP: offset[5]=inst[12], offset[4:2]=inst[6:4], offset[7:6]=inst[3:2]
static i64 imm_lwsp(u16 r) {
  return (i64)(((r >> 12) & 1) << 5 | ((r >> 4) & 7) << 2 | ((r >> 2) & 3) << 6);
}

// CISP double — C.LDSP/C.FLDSP: offset[5]=inst[12], offset[4:3]=inst[6:5], offset[8:6]=inst[4:2]
static i64 imm_ldsp(u16 r) {
  return (i64)(((r >> 12) & 1) << 5 | ((r >> 5) & 3) << 3 | ((r >> 2) & 7) << 6);
}

// CSS word — C.SWSP: offset[5:2]=inst[12:9], offset[7:6]=inst[8:7]
static i64 imm_swsp(u16 r) { return (i64)(((r >> 9) & 0xF) << 2 | ((r >> 7) & 3) << 6); }

// CSS double — C.SDSP/C.FSDSP: offset[5:3]=inst[12:10], offset[8:6]=inst[9:7]
static i64 imm_sdsp(u16 r) { return (i64)(((r >> 10) & 7) << 3 | ((r >> 7) & 7) << 6); }

// ADDI4SPN: nzuimm[5:4]=inst[12:11], nzuimm[9:6]=inst[10:7], nzuimm[2]=inst[6], nzuimm[3]=inst[5]
static i64 imm_addi4spn(u16 r) {
  return (i64)(((r >> 6) & 1) << 2 | ((r >> 5) & 1) << 3 | ((r >> 11) & 3) << 4 |
               ((r >> 7) & 0xF) << 6);
}

// ---------------------------------------------------------------------------
// decode_rvc — translate a 16-bit RVC instruction into a base Instruction
// ---------------------------------------------------------------------------
Instruction decode_rvc(u16 r) {
  Instruction inst = {.size = 2};

  u32 op = (r >> RVC_OP_SHIFT) & RVC_OP_MASK;
  u32 f3 = (r >> RVC_F3_SHIFT) & RVC_F3_MASK;

  switch (op) {

  // -------------------------------------------------------------------------
  // Quadrant 0
  // -------------------------------------------------------------------------
  case RVC_Q0:
    switch (f3) {
    case RVC_Q0_ADDI4SPN: // C.ADDI4SPN → addi rd', x2, nzuimm
      inst.opcode = OP_I_ARITH;
      inst.funct3 = F3_ADD_SUB;
      inst.rd     = rvc_rd_s(r);
      inst.rs1    = REG_SP;
      inst.imm    = imm_addi4spn(r);
      break;
    case RVC_Q0_FLD: // C.FLD → fld rd', offset(rs1')
      inst.opcode = OP_FLW_FLD;
      inst.funct3 = F3_FLD_FSD;
      inst.rd     = rvc_rd_s(r);
      inst.rs1    = rvc_rs1_s(r);
      inst.imm    = imm_cld(r);
      break;
    case RVC_Q0_LW: // C.LW → lw rd', offset(rs1')
      inst.opcode = OP_LOAD;
      inst.funct3 = F3_LW;
      inst.rd     = rvc_rd_s(r);
      inst.rs1    = rvc_rs1_s(r);
      inst.imm    = imm_clw(r);
      break;
    case RVC_Q0_LD: // C.LD → ld rd', offset(rs1')
      inst.opcode = OP_LOAD;
      inst.funct3 = F3_LD;
      inst.rd     = rvc_rd_s(r);
      inst.rs1    = rvc_rs1_s(r);
      inst.imm    = imm_cld(r);
      break;
    case RVC_Q0_FSD: // C.FSD → fsd rs2', offset(rs1')
      inst.opcode = OP_FSW_FSD;
      inst.funct3 = F3_FLD_FSD;
      inst.rs1    = rvc_rs1_s(r);
      inst.rs2    = rvc_rd_s(r);
      inst.imm    = imm_cld(r);
      break;
    case RVC_Q0_SW: // C.SW → sw rs2', offset(rs1')
      inst.opcode = OP_STORE;
      inst.funct3 = F3_SW;
      inst.rs1    = rvc_rs1_s(r);
      inst.rs2    = rvc_rd_s(r);
      inst.imm    = imm_clw(r);
      break;
    case RVC_Q0_SD: // C.SD → sd rs2', offset(rs1')
      inst.opcode = OP_STORE;
      inst.funct3 = F3_SD;
      inst.rs1    = rvc_rs1_s(r);
      inst.rs2    = rvc_rd_s(r);
      inst.imm    = imm_cld(r);
      break;
    default:
      inst.opcode = 0xFF; // illegal / reserved
      break;
    }
    break;

  // -------------------------------------------------------------------------
  // Quadrant 1
  // -------------------------------------------------------------------------
  case RVC_Q1:
    switch (f3) {
    case RVC_Q1_ADDI: // C.NOP / C.ADDI → addi rd, rd, imm
      inst.opcode = OP_I_ARITH;
      inst.funct3 = F3_ADD_SUB;
      inst.rd     = rvc_rd(r);
      inst.rs1    = inst.rd;
      inst.imm    = imm_ci(r);
      break;
    case RVC_Q1_ADDIW: // C.ADDIW → addiw rd, rd, imm  (RV64)
      inst.opcode = OP_I_ARITH_W;
      inst.funct3 = F3_ADD_SUB;
      inst.rd     = rvc_rd(r);
      inst.rs1    = inst.rd;
      inst.imm    = imm_ci(r);
      break;
    case RVC_Q1_LI: // C.LI → addi rd, x0, imm
      inst.opcode = OP_I_ARITH;
      inst.funct3 = F3_ADD_SUB;
      inst.rd     = rvc_rd(r);
      inst.rs1    = 0;
      inst.imm    = imm_ci(r);
      break;
    case RVC_Q1_LUI: {
      u32 rd = rvc_rd(r);
      if (rd == REG_SP) { // C.ADDI16SP → addi x2, x2, nzimm
        inst.opcode = OP_I_ARITH;
        inst.funct3 = F3_ADD_SUB;
        inst.rd     = REG_SP;
        inst.rs1    = REG_SP;
        inst.imm    = imm_addi16sp(r);
      } else { // C.LUI → lui rd, nzimm
        inst.opcode = OP_LUI;
        inst.rd     = rd;
        inst.imm    = imm_lui(r);
      }
      break;
    }
    case RVC_Q1_MISC_ALU: {
      u32 rd = rvc_rs1_s(r); // rd' doubles as rs1' in CB/CA format
      switch (rvc_sub(r)) {
      case RVC_ALU_SRLI: // C.SRLI → srli rd', rd', shamt
        inst.opcode = OP_I_ARITH;
        inst.funct3 = F3_SRL_SRA;
        inst.funct7 = 0x00;
        inst.rd     = rd;
        inst.rs1    = rd;
        inst.imm    = imm_ci(r) & 0x3F;
        break;
      case RVC_ALU_SRAI: // C.SRAI → srai rd', rd', shamt
        inst.opcode = OP_I_ARITH;
        inst.funct3 = F3_SRL_SRA;
        inst.funct7 = F7_ALT;
        inst.rd     = rd;
        inst.rs1    = rd;
        inst.imm    = imm_ci(r) & 0x3F;
        break;
      case RVC_ALU_ANDI: // C.ANDI → andi rd', rd', imm
        inst.opcode = OP_I_ARITH;
        inst.funct3 = F3_AND;
        inst.rd     = rd;
        inst.rs1    = rd;
        inst.imm    = imm_ci(r);
        break;
      case RVC_ALU_ARITH: { // C.SUB/XOR/OR/AND (b12=0)  C.SUBW/ADDW (b12=1)
        u32 rs2 = rvc_rd_s(r);
        if (!rvc_b12(r)) {
          inst.opcode = OP_R_ARITH;
          inst.rd     = rd;
          inst.rs1    = rd;
          inst.rs2    = rs2;
          switch (rvc_arith(r)) {
          case RVC_ARITH_SUB:
            inst.funct3 = F3_ADD_SUB;
            inst.funct7 = F7_ALT;
            break;
          case RVC_ARITH_XOR:
            inst.funct3 = F3_XOR;
            inst.funct7 = 0x00;
            break;
          case RVC_ARITH_OR:
            inst.funct3 = F3_OR;
            inst.funct7 = 0x00;
            break;
          case RVC_ARITH_AND:
            inst.funct3 = F3_AND;
            inst.funct7 = 0x00;
            break;
          }
        } else {
          inst.opcode = OP_R_ARITH_W;
          inst.rd     = rd;
          inst.rs1    = rd;
          inst.rs2    = rs2;
          switch (rvc_arith(r)) {
          case RVC_ARITH_SUB:
            inst.funct3 = F3_ADD_SUB;
            inst.funct7 = F7_ALT;
            break; // C.SUBW
          case RVC_ARITH_XOR:
            inst.funct3 = F3_ADD_SUB;
            inst.funct7 = 0x00;
            break; // C.ADDW
          default:
            inst.opcode = 0xFF;
            break; // reserved
          }
        }
        break;
      }
      }
      break;
    }
    case RVC_Q1_J: // C.J → jal x0, offset
      inst.opcode = OP_JAL;
      inst.rd     = 0;
      inst.imm    = imm_cj(r);
      break;
    case RVC_Q1_BEQZ: // C.BEQZ → beq rs1', x0, offset
      inst.opcode = OP_BRANCH;
      inst.funct3 = F3_BEQ;
      inst.rs1    = rvc_rs1_s(r);
      inst.rs2    = 0;
      inst.imm    = imm_cb(r);
      break;
    case RVC_Q1_BNEZ: // C.BNEZ → bne rs1', x0, offset
      inst.opcode = OP_BRANCH;
      inst.funct3 = F3_BNE;
      inst.rs1    = rvc_rs1_s(r);
      inst.rs2    = 0;
      inst.imm    = imm_cb(r);
      break;
    }
    break;

  // -------------------------------------------------------------------------
  // Quadrant 2
  // -------------------------------------------------------------------------
  case RVC_Q2:
    switch (f3) {
    case RVC_Q2_SLLI: // C.SLLI → slli rd, rd, shamt
      inst.opcode = OP_I_ARITH;
      inst.funct3 = F3_SLL;
      inst.rd     = rvc_rd(r);
      inst.rs1    = inst.rd;
      inst.imm    = (i64)(((r >> RVC_B12_SHIFT) & RVC_B12_MASK) << 5 |
                       ((r >> RVC_CR_RS2_SHIFT) & RVC_REG_MASK));
      break;
    case RVC_Q2_FLDSP: // C.FLDSP → fld rd, offset(x2)
      inst.opcode = OP_FLW_FLD;
      inst.funct3 = F3_FLD_FSD;
      inst.rd     = rvc_rd(r);
      inst.rs1    = REG_SP;
      inst.imm    = imm_ldsp(r);
      break;
    case RVC_Q2_LWSP: // C.LWSP → lw rd, offset(x2)
      inst.opcode = OP_LOAD;
      inst.funct3 = F3_LW;
      inst.rd     = rvc_rd(r);
      inst.rs1    = REG_SP;
      inst.imm    = imm_lwsp(r);
      break;
    case RVC_Q2_LDSP: // C.LDSP → ld rd, offset(x2)
      inst.opcode = OP_LOAD;
      inst.funct3 = F3_LD;
      inst.rd     = rvc_rd(r);
      inst.rs1    = REG_SP;
      inst.imm    = imm_ldsp(r);
      break;
    case RVC_Q2_CR: {
      u32 rs1 = rvc_rd(r);           // CR format: bits[11:7]
      u32 rs2 = rvc_rs2(r);          // CR format: bits[6:2]
      if (!rvc_b12(r) && rs2 == 0) { // C.JR → jalr x0, rs1, 0
        inst.opcode = OP_JALR;
        inst.rd     = 0;
        inst.rs1    = rs1;
        inst.imm    = 0;
      } else if (!rvc_b12(r)) { // C.MV → add rd, x0, rs2
        inst.opcode = OP_R_ARITH;
        inst.funct3 = F3_ADD_SUB;
        inst.funct7 = 0x00;
        inst.rd     = rs1;
        inst.rs1    = 0;
        inst.rs2    = rs2;
      } else if (rs1 == 0 && rs2 == 0) { // C.EBREAK
        inst.opcode = OP_SYSTEM;
        inst.funct3 = 0;
        inst.imm    = SYSTEM_EBREAK;
      } else if (rs2 == 0) { // C.JALR → jalr x1, rs1, 0
        inst.opcode = OP_JALR;
        inst.rd     = 1;
        inst.rs1    = rs1;
        inst.imm    = 0;
      } else { // C.ADD → add rd, rd, rs2
        inst.opcode = OP_R_ARITH;
        inst.funct3 = F3_ADD_SUB;
        inst.funct7 = 0x00;
        inst.rd     = rs1;
        inst.rs1    = rs1;
        inst.rs2    = rs2;
      }
      break;
    }
    case RVC_Q2_FSDSP: // C.FSDSP → fsd rs2, offset(x2)
      inst.opcode = OP_FSW_FSD;
      inst.funct3 = F3_FLD_FSD;
      inst.rs1    = REG_SP;
      inst.rs2    = rvc_rs2(r);
      inst.imm    = imm_sdsp(r);
      break;
    case RVC_Q2_SWSP: // C.SWSP → sw rs2, offset(x2)
      inst.opcode = OP_STORE;
      inst.funct3 = F3_SW;
      inst.rs1    = REG_SP;
      inst.rs2    = rvc_rs2(r);
      inst.imm    = imm_swsp(r);
      break;
    case RVC_Q2_SDSP: // C.SDSP → sd rs2, offset(x2)
      inst.opcode = OP_STORE;
      inst.funct3 = F3_SD;
      inst.rs1    = REG_SP;
      inst.rs2    = rvc_rs2(r);
      inst.imm    = imm_sdsp(r);
      break;
    }
    break;
  }

  return inst;
}
