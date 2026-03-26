#pragma once

// Exception cause codes — written to mcause/scause when interrupt bit is 0.
// Used in cpu.c (trap dispatch), mmu.c (page fault causes), sbi.c (medeleg masks).
#define EXC_CAUSE_INSTR_MISALIGNED 0x0 // instruction address misaligned
#define EXC_CAUSE_INSTR_FAULT 0x1      // instruction access fault
#define EXC_CAUSE_ILLEGAL_INSTR 0x2    // illegal instruction
#define EXC_CAUSE_BREAKPOINT 0x3       // breakpoint
#define EXC_CAUSE_LOAD_MISALIGNED 0x4  // load address misaligned
#define EXC_CAUSE_LOAD_FAULT 0x5       // load access fault
#define EXC_CAUSE_STORE_MISALIGNED 0x6 // store/AMO address misaligned
#define EXC_CAUSE_STORE_FAULT 0x7      // store/AMO access fault
#define EXC_CAUSE_ECALL_U 0x8          // environment call from U-mode
#define EXC_CAUSE_ECALL_S 0x9          // environment call from S-mode
#define EXC_CAUSE_ECALL_M 0xb          // environment call from M-mode
#define EXC_CAUSE_FETCH_PAGE_FAULT 0xc // instruction page fault
#define EXC_CAUSE_LOAD_PAGE_FAULT 0xd  // load page fault
#define EXC_CAUSE_STORE_PAGE_FAULT 0xf // store page fault
