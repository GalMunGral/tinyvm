#pragma once

#include "cpu.h"
#include "memory.h"

// Physical address where the DTB is placed in guest RAM.
// Must be reachable by the kernel — we put it well past the kernel image.
#define LINUX_DTB_ADDR 0x82000000ULL

// Load a Linux kernel ELF and DTB into guest memory, then set up the
// RISC-V Linux boot register convention:
//   a0 (x10) = hart ID (0)
//   a1 (x11) = DTB physical address
//   pc        = kernel ELF entry point
//
// Returns 0 on success, -1 on failure.
int linux_boot(Memory *mem, CPU *cpu, const char *kernel_path, const char *dtb_path);