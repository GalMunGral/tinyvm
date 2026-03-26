#include "sbi.h"

#include <stdio.h>

#include "clint.h"

// medeleg bitmasks — bit N delegates exception cause N to S-mode
#define MEDELEG_BIT(cause) ((u64)1 << (cause))

void sbi_init(CPU *cpu, const u64 kernel_entry) {
  // Delegate exceptions to S-mode (all except ecall from S-mode, which stays in M-mode for SBI)
  cpu->csrs[CSR_MEDELEG] =
      MEDELEG_BIT(EXC_CAUSE_INSTR_MISALIGNED) | MEDELEG_BIT(EXC_CAUSE_INSTR_FAULT) |
      MEDELEG_BIT(EXC_CAUSE_ILLEGAL_INSTR) | MEDELEG_BIT(EXC_CAUSE_BREAKPOINT) |
      MEDELEG_BIT(EXC_CAUSE_LOAD_MISALIGNED) | MEDELEG_BIT(EXC_CAUSE_LOAD_FAULT) |
      MEDELEG_BIT(EXC_CAUSE_STORE_MISALIGNED) | MEDELEG_BIT(EXC_CAUSE_STORE_FAULT) |
      MEDELEG_BIT(EXC_CAUSE_ECALL_U) | MEDELEG_BIT(EXC_CAUSE_FETCH_PAGE_FAULT) |
      MEDELEG_BIT(EXC_CAUSE_LOAD_PAGE_FAULT) | MEDELEG_BIT(EXC_CAUSE_STORE_PAGE_FAULT);

  // Delegate timer, software, and external interrupts to S-mode
  cpu->csrs[CSR_MIDELEG] = MIP_SSIP | MIP_STIP | MIP_SEIP;

  // Allow S-mode to read cycle, time, and instret directly via rdcycle/rdtime/rdinstret
  cpu->csrs[CSR_MCOUNTEREN] = MCOUNTEREN_CY | MCOUNTEREN_TM | MCOUNTEREN_IR;

  // Simulate mret: MPP=S, MPIE=1, drop privilege to S-mode, jump to kernel
  u64 status = cpu->csrs[CSR_MSTATUS];
  status &= ~MSTATUS_MPP_MASK;
  status |= ((u64)PRIV_S << MSTATUS_MPP_SHIFT);
  status |= MSTATUS_MPIE;
  cpu->csrs[CSR_MSTATUS] = status;

  cpu->pc        = kernel_entry;
  cpu->privilege = PRIV_S;
}

// ---------------------------------------------------------------------------
// Base extension helpers
// ---------------------------------------------------------------------------

static bool is_supported_eid(u64 eid) {
  switch (eid) {
  case SBI_EID_SET_TIMER:
    return true;
  case SBI_EID_PUTCHAR:
    return true;
  case SBI_EID_BASE:
    return true;
  case SBI_EID_TIMER:
    return true;
  case SBI_EID_RESET:
    return true;
  default:
    return false;
  }
}

static void handle_base(CPU *cpu) {
  u64 fid = cpu->regs[REG_A6];
  switch (fid) {
  case SBI_BASE_GET_SPEC_VERSION:
    cpu->regs[REG_A0] = SBI_SUCCESS;
    cpu->regs[REG_A1] = SBI_SPEC_VERSION;
    break;
  case SBI_BASE_PROBE_EXTENSION: {
    u64 probe_eid     = cpu->regs[REG_A0];
    cpu->regs[REG_A0] = SBI_SUCCESS;
    cpu->regs[REG_A1] = is_supported_eid(probe_eid) ? 1 : 0;
    break;
  }
  default:
    cpu->regs[REG_A0] = SBI_ERR_NOT_SUPPORTED;
    cpu->regs[REG_A1] = 0;
    break;
  }
}

// ---------------------------------------------------------------------------
// sbi_ecall — handle an SBI ecall from S-mode
// ---------------------------------------------------------------------------

void sbi_ecall(CPU *cpu, const Memory *mem) {
  (void)mem;
  u64 eid = cpu->regs[REG_A7];

  switch (eid) {
  case SBI_EID_SET_TIMER:
    cpu_lower_irq(cpu, MIP_STIP);
    clint_set_timecmp(cpu->regs[REG_A0]);
    cpu->regs[REG_A0] = SBI_SUCCESS;
    cpu->regs[REG_A1] = 0;
    break;
  case SBI_EID_TIMER:
    if (cpu->regs[REG_A6] == SBI_TIMER_SET_TIMER) {
      cpu_lower_irq(cpu, MIP_STIP);
      clint_set_timecmp(cpu->regs[REG_A0]);
      cpu->regs[REG_A0] = SBI_SUCCESS;
      cpu->regs[REG_A1] = 0;
    } else {
      cpu->regs[REG_A0] = SBI_ERR_NOT_SUPPORTED;
      cpu->regs[REG_A1] = 0;
    }
    break;
  case SBI_EID_PUTCHAR:
    putchar((int)cpu->regs[REG_A0]);
    fflush(stdout);
    cpu->regs[REG_A0] = SBI_SUCCESS;
    cpu->regs[REG_A1] = 0;
    break;
  case SBI_EID_BASE:
    handle_base(cpu);
    break;
  case SBI_EID_RESET:
    cpu->halted       = true;
    cpu->regs[REG_A0] = SBI_SUCCESS;
    cpu->regs[REG_A1] = 0;
    break;
  default:
    cpu->regs[REG_A0] = SBI_ERR_NOT_SUPPORTED;
    cpu->regs[REG_A1] = 0;
    break;
  }

  cpu->pc += INSN_SIZE;
}