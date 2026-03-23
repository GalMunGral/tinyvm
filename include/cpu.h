#pragma once

#include "types.h"
#include "memory.h"

#define NUM_REGS 32
#define NUM_CSRS 4096

// Privilege levels
#define PRIV_U 0
#define PRIV_S 1
#define PRIV_M 3

// CSR addresses — machine mode
#define CSR_MSTATUS 0x300
#define CSR_MEDELEG 0x302
#define CSR_MIDELEG 0x303
#define CSR_MIE 0x304
#define CSR_MTVEC 0x305
#define CSR_MSCRATCH 0x340
#define CSR_MEPC 0x341
#define CSR_MCAUSE 0x342
#define CSR_MTVAL 0x343
#define CSR_MIP 0x344

// CSR addresses — supervisor mode
#define CSR_SSTATUS 0x100
#define CSR_SIE 0x104
#define CSR_STVEC 0x105
#define CSR_SSCRATCH 0x140
#define CSR_SEPC 0x141
#define CSR_SCAUSE 0x142
#define CSR_STVAL 0x143
#define CSR_SIP 0x144
#define CSR_SATP 0x180

// mstatus bit positions
#define MSTATUS_SIE (1ULL << 1)  // S-mode interrupt enable
#define MSTATUS_MIE (1ULL << 3)  // M-mode interrupt enable
#define MSTATUS_SPIE (1ULL << 5) // previous S-mode interrupt enable (saved on S-mode trap entry)
#define MSTATUS_MPIE (1ULL << 7) // previous M-mode interrupt enable (saved on M-mode trap entry)
#define MSTATUS_SPP (1ULL << 8)  // previous privilege before S-mode trap (0=U, 1=S)

// MPP is a 2-bit field at bits 12:11 — previous privilege before M-mode trap (0=U, 1=S, 3=M)
#define MSTATUS_MPP_SHIFT 11

typedef struct CPU {
  u64 regs[NUM_REGS];
  u64 fregs[NUM_REGS];
  u32 fcsr;
  u64 pc;
  u64 csrs[NUM_CSRS];
  u8  privilege;
} CPU;

CPU *cpu_create(void);
void cpu_destroy(CPU *cpu);
void cpu_step(CPU *cpu, const Memory *mem);