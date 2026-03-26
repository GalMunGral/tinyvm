#pragma once

#include "cpu.h"
#include "memory.h"

// Physical addresses in guest RAM for boot artifacts.
// All must fall within the 256MB RAM region (0x7f000000–0x8f000000).
#define LINUX_DTB_ADDR 0x82000000ULL
#define LINUX_INITRAMFS_ADDR 0x84000000ULL

// Load a Linux kernel ELF, DTB, and initramfs into guest memory, then set up
// the RISC-V Linux boot register convention:
//   a0 (x10) = hart ID (0)
//   a1 (x11) = DTB physical address
//   pc        = kernel ELF entry point
//
// Returns 0 on success, -1 on failure.
int linux_boot(Memory *mem, CPU *cpu, const char *kernel_path, const char *dtb_path,
               const char *initramfs_path);