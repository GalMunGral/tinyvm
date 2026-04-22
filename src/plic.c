#include "plic.h"

#define PLIC_NUM_SOURCES 32
#define PLIC_NUM_CONTEXTS 2

// Context indices: one per hart x privilege-level pair
#define PLIC_CTX_M 0 // hart 0, M-mode external
#define PLIC_CTX_S 1 // hart 0, S-mode external

// MMIO region offsets
#define PLIC_PRIORITY_OFF 0x000000       // + 4*irq
#define PLIC_PENDING_OFF 0x001000
#define PLIC_ENABLE_OFF 0x002000         // + PLIC_ENABLE_STRIDE*ctx
#define PLIC_ENABLE_STRIDE 0x80
#define PLIC_THRESHOLD_OFF 0x200000      // + PLIC_CONTEXT_STRIDE*ctx
#define PLIC_CONTEXT_STRIDE 0x1000
#define PLIC_CLAIM_CTX_OFF 0x4           // offset of claim/complete within a context block

static struct {
  u32 priority[PLIC_NUM_SOURCES];
  u32 pending;
  u32 enable[PLIC_NUM_CONTEXTS];
  u32 threshold[PLIC_NUM_CONTEXTS];
  u32 claimed;
  CPU *cpu;
} s_plic;

// Re-evaluate whether SEIP should be asserted for the S-mode context.
static void plic_update_seip(void) {
  u32 actionable = s_plic.pending & s_plic.enable[PLIC_CTX_S] & ~s_plic.claimed;
  bool fire = false;
  for (int irq = 1; irq < PLIC_NUM_SOURCES; irq++) {
    if ((actionable & (1u << irq)) && s_plic.priority[irq] > s_plic.threshold[PLIC_CTX_S]) {
      fire = true;
      break;
    }
  }
  if (fire)
    cpu_raise_irq(s_plic.cpu, MIP_SEIP);
  else
    cpu_lower_irq(s_plic.cpu, MIP_SEIP);
}

void plic_set_pending(u32 irq) {
  if (irq == 0 || irq >= PLIC_NUM_SOURCES)
    return;
  s_plic.pending |= (1u << irq);
  plic_update_seip();
}

// Return the highest-priority claimable IRQ for context ctx, or 0 if none.
static u32 plic_claim(int ctx) {
  u32 actionable = s_plic.pending & s_plic.enable[ctx] & ~s_plic.claimed;
  u32 best_irq = 0;
  u32 best_pri = 0;
  for (int irq = 1; irq < PLIC_NUM_SOURCES; irq++) {
    if ((actionable & (1u << irq)) && s_plic.priority[irq] > s_plic.threshold[ctx] &&
        s_plic.priority[irq] > best_pri) {
      best_pri = s_plic.priority[irq];
      best_irq = (u32)irq;
    }
  }
  if (best_irq) {
    s_plic.claimed |= (1u << best_irq);
    s_plic.pending &= ~(1u << best_irq);
    plic_update_seip();
  }
  return best_irq;
}

static void plic_complete(u32 irq) {
  if (irq == 0 || irq >= PLIC_NUM_SOURCES)
    return;
  s_plic.claimed &= ~(1u << irq);
  plic_update_seip();
}

static u64 plic_read(MemRegion *r, u64 offset, size_t width) {
  (void)r;
  (void)width;

  // Priority registers: base + 4*irq
  if (offset < PLIC_PENDING_OFF) {
    u32 irq = (u32)(offset / 4);
    if (irq < PLIC_NUM_SOURCES)
      return s_plic.priority[irq];
    return 0;
  }

  // Pending bits (read-only)
  if (offset == PLIC_PENDING_OFF)
    return s_plic.pending;

  // Enable bits: PLIC_ENABLE_OFF + PLIC_ENABLE_STRIDE*ctx
  if (offset >= PLIC_ENABLE_OFF && offset < PLIC_THRESHOLD_OFF) {
    int ctx = (int)((offset - PLIC_ENABLE_OFF) / PLIC_ENABLE_STRIDE);
    if (ctx < PLIC_NUM_CONTEXTS && ((offset - PLIC_ENABLE_OFF) % PLIC_ENABLE_STRIDE) == 0)
      return s_plic.enable[ctx];
    return 0;
  }

  // Threshold and claim: PLIC_THRESHOLD_OFF + PLIC_CONTEXT_STRIDE*ctx
  if (offset >= PLIC_THRESHOLD_OFF) {
    int ctx = (int)((offset - PLIC_THRESHOLD_OFF) / PLIC_CONTEXT_STRIDE);
    u64 ctx_off = (offset - PLIC_THRESHOLD_OFF) % PLIC_CONTEXT_STRIDE;
    if (ctx < PLIC_NUM_CONTEXTS) {
      if (ctx_off == 0)
        return s_plic.threshold[ctx];
      if (ctx_off == PLIC_CLAIM_CTX_OFF)
        return plic_claim(ctx);
    }
  }

  return 0;
}

static void plic_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)r;
  (void)width;

  // Priority registers
  if (offset < PLIC_PENDING_OFF) {
    u32 irq = (u32)(offset / 4);
    if (irq < PLIC_NUM_SOURCES) {
      s_plic.priority[irq] = (u32)val;
      plic_update_seip();
    }
    return;
  }

  // Pending is read-only
  if (offset == PLIC_PENDING_OFF)
    return;

  // Enable bits
  if (offset >= PLIC_ENABLE_OFF && offset < PLIC_THRESHOLD_OFF) {
    int ctx = (int)((offset - PLIC_ENABLE_OFF) / PLIC_ENABLE_STRIDE);
    if (ctx < PLIC_NUM_CONTEXTS && ((offset - PLIC_ENABLE_OFF) % PLIC_ENABLE_STRIDE) == 0) {
      s_plic.enable[ctx] = (u32)val;
      plic_update_seip();
    }
    return;
  }

  // Threshold and complete
  if (offset >= PLIC_THRESHOLD_OFF) {
    int ctx = (int)((offset - PLIC_THRESHOLD_OFF) / PLIC_CONTEXT_STRIDE);
    u64 ctx_off = (offset - PLIC_THRESHOLD_OFF) % PLIC_CONTEXT_STRIDE;
    if (ctx < PLIC_NUM_CONTEXTS) {
      if (ctx_off == 0) {
        s_plic.threshold[ctx] = (u32)val;
        plic_update_seip();
      } else if (ctx_off == PLIC_CLAIM_CTX_OFF) {
        plic_complete((u32)val);
      }
    }
  }
}

void plic_init(Memory *mem, CPU *cpu) {
  s_plic.cpu = cpu;
  mem_add_device(mem, PLIC_BASE, PLIC_SIZE, plic_read, plic_write, NULL);
}