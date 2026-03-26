#pragma once

#include "memory.h"
#include "trap.h"
#include "types.h"

#define NUM_REGS 32
#define NUM_CSRS 4096
#define INSN_SIZE 4 // all base ISA instructions are 4 bytes (no C extension)

// Privilege levels
typedef enum { PRIV_U = 0, PRIV_S = 1, PRIV_M = 3 } Privilege;

// Integer register ABI names (RISC-V calling convention)
typedef enum {
  REG_ZERO = 0, // zero — hardwired zero
  REG_RA   = 1, // ra   — return address
  REG_SP   = 2, // sp   — stack pointer
  REG_GP   = 3, // gp   — global pointer
  REG_TP   = 4, // tp   — thread pointer
  REG_T0   = 5,
  REG_T1   = 6,
  REG_T2   = 7, // t0–t2  — temporaries
  REG_S0   = 8,
  REG_S1   = 9, // s0–s1  — saved registers (s0 = fp)
  REG_A0   = 10,
  REG_A1   = 11,
  REG_A2   = 12, // a0–a2  — args / return values
  REG_A3   = 13,
  REG_A4   = 14,
  REG_A5   = 15, // a3–a5  — args
  REG_A6   = 16,
  REG_A7   = 17, // a6–a7  — args (a7 = syscall/SBI EID)
  REG_S2   = 18,
  REG_S3   = 19,
  REG_S4   = 20,
  REG_S5   = 21, // s2–s5  — saved registers
  REG_S6   = 22,
  REG_S7   = 23,
  REG_S8   = 24,
  REG_S9   = 25, // s6–s9  — saved registers
  REG_S10  = 26,
  REG_S11  = 27, // s10–s11 — saved registers
  REG_T3   = 28,
  REG_T4   = 29,
  REG_T5   = 30,
  REG_T6   = 31, // t3–t6  — temporaries
} RegName;

// CSR addresses — machine mode
#define CSR_MSTATUS 0x300
#define CSR_MEDELEG 0x302
#define CSR_MIDELEG 0x303
#define CSR_MIE 0x304
#define CSR_MTVEC 0x305
#define CSR_MCOUNTEREN 0x306
#define CSR_MSCRATCH 0x340
#define CSR_MEPC 0x341
#define CSR_MCAUSE 0x342
#define CSR_MTVAL 0x343
#define CSR_MIP 0x344

// mcounteren bits — allow S/U-mode to read cycle/time/instret
#define MCOUNTEREN_CY ((u64)1 << 0)
#define MCOUNTEREN_TM ((u64)1 << 1)
#define MCOUNTEREN_IR ((u64)1 << 2)

// CSR addresses — counters (read-only from U/S-mode; machine aliases at 0xB__)
#define CSR_CYCLE 0xC00    // user-mode cycle counter (read-only shadow)
#define CSR_TIME 0xC01     // user-mode real-time timer (read-only shadow)
#define CSR_INSTRET 0xC02  // user-mode instructions-retired (read-only shadow)
#define CSR_MCYCLE 0xB00   // machine cycle counter
#define CSR_MINSTRET 0xB02 // machine instructions-retired

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
#define MSTATUS_SIE ((u64)1 << 1)  // S-mode interrupt enable
#define MSTATUS_MIE ((u64)1 << 3)  // M-mode interrupt enable
#define MSTATUS_SPIE ((u64)1 << 5) // previous S-mode interrupt enable (saved on S-mode trap entry)
#define MSTATUS_MPIE ((u64)1 << 7) // previous M-mode interrupt enable (saved on M-mode trap entry)
#define MSTATUS_SPP ((u64)1 << 8)  // previous privilege before S-mode trap (0=U, 1=S)

// sstatus is a restricted view of mstatus — only these bits are visible to S-mode
#define SSTATUS_SIE ((u64)1 << 1)  // S-mode interrupt enable
#define SSTATUS_SPIE ((u64)1 << 5) // previous S-mode interrupt enable
#define SSTATUS_SPP ((u64)1 << 8)  // previous privilege before S-mode trap
#define SSTATUS_FS ((u64)3 << 13)  // floating-point unit state
#define SSTATUS_SUM ((u64)1 << 18) // supervisor user memory access
#define SSTATUS_MXR ((u64)1 << 19) // make executable pages readable
#define SSTATUS_MASK                                                                               \
  (SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP | SSTATUS_FS | SSTATUS_SUM | SSTATUS_MXR)

// MPP is a 2-bit field at bits 12:11 — previous privilege before M-mode trap (0=U, 1=S, 3=M)
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_MPP_MASK ((u64)0x3 << MSTATUS_MPP_SHIFT)

// satp mode field — bits[63:60]
#define SATP_MODE_SHIFT 60
#define SATP_MODE_BARE 0 // no translation
#define SATP_MODE_SV39 8 // 3-level page tables (supported)
#define SATP_MODE_MASK ((u64)0xF << SATP_MODE_SHIFT)

// mip/mie bit positions (same indices used in both registers)
#define MIP_SSIP ((u64)1 << 1)  // S-mode software interrupt
#define MIP_MSIP ((u64)1 << 3)  // M-mode software interrupt
#define MIP_STIP ((u64)1 << 5)  // S-mode timer interrupt
#define MIP_MTIP ((u64)1 << 7)  // M-mode timer interrupt (set by CLINT hardware)
#define MIP_SEIP ((u64)1 << 9)  // S-mode external interrupt
#define MIP_MEIP ((u64)1 << 11) // M-mode external interrupt

typedef struct CPU {
  u64       regs[NUM_REGS];
  u64       fregs[NUM_REGS];
  u32       fcsr;
  u64       pc;
  u64       csrs[NUM_CSRS];
  Privilege privilege;
  bool      halted;
  u64       steps;
} CPU;

void cpu_init(CPU *cpu);
void cpu_step(CPU *cpu, const Memory *mem);
void cpu_trap(CPU *cpu, u64 cause, u64 tval);

// Raise/lower an interrupt line — the hardware-model contract for peripherals
void cpu_raise_irq(CPU *cpu, u64 bit);
void cpu_lower_irq(CPU *cpu, u64 bit);

// Register a per-step poll callback — emulation artifact for time-varying peripherals (e.g. CLINT)
// In real hardware these peripherals have dedicated comparison logic running continuously;
// in our emulator we approximate that by calling the source function every cpu_step.
typedef void (*IrqSourceFn)(CPU *cpu);
void cpu_add_irq_source(IrqSourceFn fn);
