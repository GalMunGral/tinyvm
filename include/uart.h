#pragma once

#include "memory.h"

#define UART_BASE 0x10000000ULL

void uart_init(Memory *mem);