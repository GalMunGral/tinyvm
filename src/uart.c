#include <stdio.h>
#include "uart.h"

#define UART_SIZE 8

// NS16550 register offsets
#define UART_THR 0 // transmit holding register (write)
#define UART_RBR 0 // receiver buffer register (read)
#define UART_LSR 5 // line status register

// LSR bits: TX holding register empty | TX empty
#define UART_LSR_TX_READY 0x60

static u64 uart_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)width;
  if (offset == UART_LSR)
    return UART_LSR_TX_READY; // always ready to transmit
  return 0;
}

static void uart_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)width;
  if (offset == UART_THR) {
    putchar((int)(val & 0xFF));
    fflush(stdout);
  }
}

void uart_init(Memory *mem) { mem_add_device(mem, UART_BASE, UART_SIZE, uart_read, uart_write); }
