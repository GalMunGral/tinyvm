#include "uart.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

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
#define UART_LSR_RX_READY 0x01 // DR  (bit0): receiver data ready
#define UART_LSR_TX_READY 0x60 // THRE (bit5) + TEMT (bit6): always ready to transmit

// IIR bits
#define UART_IIR_NO_INT 0xC1 // FIFO enabled (7:6=11), no interrupt pending (bit0=1)
#define UART_IIR_THRE 0xC2   // FIFO enabled (7:6=11), THRE type (3:1=001), pending (bit0=0)
#define UART_IIR_RDA 0xC4    // FIFO enabled (7:6=11), RDA  type (3:1=010), pending (bit0=0)

// IER bits
#define IER_RDA_ENABLE 0x01  // bit0: received data available interrupt enable
#define IER_THRE_ENABLE 0x02 // bit1: TX holding register empty interrupt enable

// LCR bit
#define UART_LCR_DLAB 0x80 // divisor latch access bit

// Modem status: CTS, DSR, DCD asserted (bits 4,5,7)
#define UART_MSR_READY 0xB0

static struct {
  u8   lcr, ier, mcr, scr, dll, dlh;
  u8   rx_buf;
  bool rx_ready;
} s_uart;

static struct termios s_orig_termios;
static int            s_orig_flags;

static void __attribute__((unused)) uart_restore(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_orig_termios);
  fcntl(STDIN_FILENO, F_SETFL, s_orig_flags);
}

static void uart_poll_rx(void) {
  if (s_uart.rx_ready)
    return;
  u8  c;
  int n = (int)read(STDIN_FILENO, &c, 1);
  if (n == 1) {
    if (c == 0x1C) // Ctrl-\ : exit emulator
      exit(0);
    s_uart.rx_buf   = c;
    s_uart.rx_ready = true;
  }
}

static u64 uart_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)width;
  switch (offset) {
  case UART_RBR:
    if (s_uart.lcr & UART_LCR_DLAB)
      return s_uart.dll;
    if (!s_uart.rx_ready)
      return 0;
    s_uart.rx_ready = false;
    return s_uart.rx_buf;
  case UART_IER:
    return (s_uart.lcr & UART_LCR_DLAB) ? s_uart.dlh : s_uart.ier;
  case UART_IIR:
    uart_poll_rx();
    if (s_uart.rx_ready && (s_uart.ier & IER_RDA_ENABLE))
      return UART_IIR_RDA;
    if (s_uart.ier & IER_THRE_ENABLE)
      return UART_IIR_THRE;
    return UART_IIR_NO_INT;
  case UART_LCR:
    return s_uart.lcr;
  case UART_MCR:
    return s_uart.mcr;
  case UART_LSR:
    uart_poll_rx();
    return UART_LSR_TX_READY | (s_uart.rx_ready ? UART_LSR_RX_READY : 0);
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

void uart_init(Memory *mem) {
  // Make stdin non-blocking so uart_poll_rx never stalls the emulator
  s_orig_flags = fcntl(STDIN_FILENO, F_GETFL);
  fcntl(STDIN_FILENO, F_SETFL, s_orig_flags | O_NONBLOCK);
  atexit(uart_restore);

  // Raw mode: single keypresses, no echo, no line buffering
  tcgetattr(STDIN_FILENO, &s_orig_termios);
  struct termios raw = s_orig_termios;
  cfmakeraw(&raw);
  raw.c_oflag = s_orig_termios.c_oflag; // preserve ONLCR so \n -> \r\n still works
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  mem_add_device(mem, UART_BASE, UART_SIZE, uart_read, uart_write);
}