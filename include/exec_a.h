#pragma once

#include "exec.h"

// Execute an A-extension instruction (opcode 0x2F: LR, SC, AMOs)
void execute_a(CPU *cpu, const Memory *mem, Instruction inst);