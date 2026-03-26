#pragma once

// ---------------------------------------------------------------------------
// Opcodes (bits[6:0])
// ---------------------------------------------------------------------------
#define OP_LUI 0x37
#define OP_AUIPC 0x17
#define OP_JAL 0x6F
#define OP_JALR 0x67
#define OP_BRANCH 0x63
#define OP_LOAD 0x03
#define OP_STORE 0x23
#define OP_I_ARITH 0x13
#define OP_R_ARITH 0x33
#define OP_I_ARITH_W 0x1B
#define OP_R_ARITH_W 0x3B
#define OP_SYSTEM 0x73
#define OP_FENCE 0x0F   // FENCE / FENCE.I
#define OP_AMO 0x2F     // A extension: LR/SC and AMOs
#define OP_FLW_FLD 0x07 // float loads
#define OP_FSW_FSD 0x27 // float stores
#define OP_FMADD 0x43   // rd = rs1*rs2 + rs3
#define OP_FMSUB 0x47   // rd = rs1*rs2 - rs3
#define OP_FNMSUB 0x4B  // rd = -(rs1*rs2) + rs3
#define OP_FNMADD 0x4F  // rd = -(rs1*rs2) - rs3
#define OP_FP 0x53      // all other float ops

// ---------------------------------------------------------------------------
// OP_SYSTEM funct3==0 imm values (bits[31:20] of instruction)
// ---------------------------------------------------------------------------
#define SYSTEM_ECALL 0x000
#define SYSTEM_EBREAK 0x001
#define SYSTEM_WFI 0x105  // wait for interrupt — NOP in emulator
#define SYSTEM_SRET 0x102 // supervisor return
#define SYSTEM_MRET 0x302 // machine return
// sfence.vma funct7 (bits[31:25], i.e. imm>>5) — rs1/rs2 vary so can't match full imm
#define SYSTEM_FUNCT7_SFENCE_VMA 0x09 // TLB flush — NOP in emulator (no TLB)

// ---------------------------------------------------------------------------
// funct3 — branches
// ---------------------------------------------------------------------------
#define F3_BEQ 0x0
#define F3_BNE 0x1
#define F3_BLT 0x4
#define F3_BGE 0x5
#define F3_BLTU 0x6
#define F3_BGEU 0x7

// ---------------------------------------------------------------------------
// funct3 — loads
// ---------------------------------------------------------------------------
#define F3_LB 0x0
#define F3_LH 0x1
#define F3_LW 0x2
#define F3_LD 0x3
#define F3_LBU 0x4
#define F3_LHU 0x5
#define F3_LWU 0x6

// ---------------------------------------------------------------------------
// funct3 — stores
// ---------------------------------------------------------------------------
#define F3_SB 0x0
#define F3_SH 0x1
#define F3_SW 0x2
#define F3_SD 0x3

// ---------------------------------------------------------------------------
// funct3 — integer arithmetic (I-type and R-type share funct3; funct7 distinguishes)
// ---------------------------------------------------------------------------
#define F3_ADD_SUB 0x0
#define F3_SLL 0x1
#define F3_SLT 0x2
#define F3_SLTU 0x3
#define F3_XOR 0x4
#define F3_SRL_SRA 0x5
#define F3_OR 0x6
#define F3_AND 0x7

// funct7 alternates
#define F7_ALT 0x20  // encodes SUB, SRA, SRAI vs ADD, SRL, SRLI
#define F7_MEXT 0x01 // M extension (multiply/divide)

// ---------------------------------------------------------------------------
// funct3 — M extension
// ---------------------------------------------------------------------------
#define F3_MUL 0x0
#define F3_MULH 0x1
#define F3_MULHSU 0x2
#define F3_MULHU 0x3
#define F3_DIV 0x4
#define F3_DIVU 0x5
#define F3_REM 0x6
#define F3_REMU 0x7

// ---------------------------------------------------------------------------
// funct3 — float loads/stores
// ---------------------------------------------------------------------------
#define F3_FLW_FSW 0x2 // single precision
#define F3_FLD_FSD 0x3 // double precision

// ---------------------------------------------------------------------------
// funct5 for OP_FP (bits[31:27] = funct7 >> 2)
// ---------------------------------------------------------------------------
#define F5_FADD 0x00
#define F5_FSUB 0x01
#define F5_FMUL 0x02
#define F5_FDIV 0x03
#define F5_FSQRT 0x0B
#define F5_FSGNJ 0x04
#define F5_FMINMAX 0x05
#define F5_FCVT_FF 0x08 // float<->float
#define F5_FCVT_FI 0x18 // float->int
#define F5_FCVT_IF 0x1A // int->float
#define F5_FMV_XI 0x1C  // float reg -> int reg (bitwise) or fclass
#define F5_FCMP 0x14    // feq/flt/fle
#define F5_FMV_IX 0x1E  // int reg -> float reg (bitwise)

// funct3 — float comparisons
#define F3_FEQ 0x2
#define F3_FLT 0x1
#define F3_FLE 0x0

// funct3 — fsgnj variants
#define F3_FSGNJ 0x0
#define F3_FSGNJN 0x1
#define F3_FSGNJX 0x2

// funct3 — fmin/fmax
#define F3_FMIN 0x0
#define F3_FMAX 0x1

// fmt field (bits[26:25] = funct7 & 0x3)
#define FMT_SINGLE 0x0
#define FMT_DOUBLE 0x1

// ---------------------------------------------------------------------------
// A-extension funct5 codes (bits[31:27] = funct7 >> 2)
// ---------------------------------------------------------------------------
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

// funct3 AMO width codes
#define F3_AMO_W 0x2 // 32-bit
#define F3_AMO_D 0x3 // 64-bit