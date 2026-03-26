#pragma once

#include "cpu.h"
#include "memory.h"

// SBI Extension IDs (a7)
#define SBI_EID_SET_TIMER 0x00ULL // legacy timer (SBI v0.1)
#define SBI_EID_PUTCHAR 0x01ULL
#define SBI_EID_BASE 0x10ULL
#define SBI_EID_TIMER 0x54494D45ULL // "TIME" — SBI v0.2 timer extension
#define SBI_EID_RESET 0x53525354ULL

// SBI Timer extension function IDs (a6, when EID=SBI_EID_TIMER)
#define SBI_TIMER_SET_TIMER 0

// SBI Base extension Function IDs (a6, when EID=SBI_EID_BASE)
#define SBI_BASE_GET_SPEC_VERSION 0
#define SBI_BASE_PROBE_EXTENSION 3

// SBI spec version we advertise: major=0, minor=2
#define SBI_SPEC_VERSION 0x00000002ULL

// SBI error codes (returned in a0)
// Defined as u64 to match register width — no cast needed at use sites
#define SBI_SUCCESS ((u64)0)
#define SBI_ERR_NOT_SUPPORTED ((u64) - 2)

// Set up M-mode delegation and drop into S-mode at the kernel entry point.
// Called once from linux_boot after the kernel and DTB are loaded.
void sbi_init(CPU *cpu, const u64 kernel_entry);

// Handle an SBI ecall from S-mode.
// Reads a7/a6/a0–a5, writes results to a0/a1, advances pc past the ecall.
// Called from cpu.c when ecall cause == 9 (ecall from S-mode).
void sbi_ecall(CPU *cpu, const Memory *mem);
