#include "uart.h"

#include <stdio.h>

#define UART_SIZE 8

// NS16550 register offsets
#define UART_THR 0 // transmit holding register (write, DLAB=0)
#define UART_RBR 0 // receiver buffer register  (read,  DLAB=0)
#define UART_DLL 0 // baud divisor low byte      (r/w,   DLAB=1)
#define UART_IER 1 // interrupt enable register  (r/w,   DLAB=0)
#define UART_DLH 1 // baud divisor high byte     (r/w,   DLAB=1)
#define UART_IIR 2 // interrupt identification register (read)
#define UART_FCR 2 // FIFO control register            (write)
#define UART_LCR 3 // line control register
#define UART_MCR 4 // modem control register
#define UART_LSR 5 // line status register
#define UART_MSR 6 // modem status register
#define UART_SCR 7 // scratch register

// LSR bits
#define UART_LSR_TX_READY 0x60 // THRE (bit5) + TEMT (bit6): always ready to transmit

// IIR bits: FIFO enabled (bits 7:6=11), no interrupt pending (bit0=1)
#define UART_IIR_NO_INT 0xC1

// LCR bit
#define UART_LCR_DLAB 0x80 // divisor latch access bit

// Modem status: CTS, DSR, DCD asserted (bits 4,5,7)
#define UART_MSR_READY 0xB0

static struct {
  u8 lcr, ier, mcr, scr, dll, dlh;
} s_uart;

static u64 uart_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)width;
  switch (offset) {
  case UART_RBR:
    return (s_uart.lcr & UART_LCR_DLAB) ? s_uart.dll : 0;
  case UART_IER:
    return (s_uart.lcr & UART_LCR_DLAB) ? s_uart.dlh : s_uart.ier;
  case UART_IIR:
    return UART_IIR_NO_INT;
  case UART_LCR:
    return s_uart.lcr;
  case UART_MCR:
    return s_uart.mcr;
  case UART_LSR:
    return UART_LSR_TX_READY;
  case UART_MSR:
    return UART_MSR_READY;
  case UART_SCR:
    return s_uart.scr;
  default:
    return 0;
  }
}

static void uart_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)width;
  u8 v = (u8)(val & 0xFF);
  switch (offset) {
  case UART_THR:
    if (s_uart.lcr & UART_LCR_DLAB) {
      s_uart.dll = v;
      break;
    }
    putchar((int)v);
    fflush(stdout);
    break;
  case UART_IER:
    if (s_uart.lcr & UART_LCR_DLAB) {
      s_uart.dlh = v;
      break;
    }
    s_uart.ier = v;
    break;
  case UART_FCR:
    break; // FIFO control — absorb, no FIFO to manage
  case UART_LCR:
    s_uart.lcr = v;
    break;
  case UART_MCR:
    s_uart.mcr = v;
    break;
  case UART_SCR:
    s_uart.scr = v;
    break;
  default:
    break;
  }
}

void uart_init(Memory *mem) { mem_add_device(mem, UART_BASE, UART_SIZE, uart_read, uart_write); }
