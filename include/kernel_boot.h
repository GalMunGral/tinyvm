#pragma once

#include "cpu.h"
#include "memory.h"

// Load a bare-metal kernel ELF and drop into S-mode at its entry point.
// Unlike linux_boot, no DTB or initramfs is required.
// a0 = hart ID (0), a1 = 0.
int kernel_boot(Memory *mem, CPU *cpu, const char *kernel_path);