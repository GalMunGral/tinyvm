#include "virtio.h"

#include <stdio.h>
#include <string.h>

#include "plic.h"

#define VIRTIO_MAGIC 0x74726976     // "virt"
#define VIRTIO_VENDOR_ID 0x554D4551 // "QEMU"
#define VIRTIO_VER 2

#define VIRTQUEUE_MAX_SIZE 128
#define VIRTQ_DESC_SIZE 16 // bytes per descriptor in guest RAM

// Ring layout offsets: u16 flags | u16 idx | entries[]
#define RING_IDX_OFF 2
#define RING_ENTRIES_OFF 4
#define AVAIL_ENTRY_SIZE 2 // u16 per entry
#define USED_ENTRY_SIZE 8  // { u32 id, u32 len } per entry

#define MAX_VIRTIO_DEVICES 8

static VirtioSlot s_slots[MAX_VIRTIO_DEVICES];
static int        s_slot_count;

// ---------------------------------------------------------------------------
// Descriptor table
// ---------------------------------------------------------------------------

VirtqDesc vq_desc_read(const Memory *mem, u64 desc_base, u16 index) {
  u64 base = desc_base + (u64)index * VIRTQ_DESC_SIZE;
  return (VirtqDesc){
      .addr  = mem_read64(mem, base),
      .len   = mem_read32(mem, base + 8),
      .flags = mem_read16(mem, base + 12),
      .next  = mem_read16(mem, base + 14),
  };
}

// ---------------------------------------------------------------------------
// Ring buffer operations
// ---------------------------------------------------------------------------

bool vq_ring_has_next(const Memory *mem, VirtqRing *ring) {
  u16 guest_idx = (u16)mem_read16(mem, ring->base + RING_IDX_OFF);
  return ring->cursor != guest_idx;
}

u16 vq_ring_next(const Memory *mem, VirtqRing *ring) {
  u16 slot = ring->cursor % ring->size;
  u64 addr = ring->base + RING_ENTRIES_OFF + (u64)slot * AVAIL_ENTRY_SIZE;
  ring->cursor++;
  return (u16)mem_read16(mem, addr);
}

void vq_ring_post(const Memory *mem, VirtqRing *ring, u16 id, u32 len) {
  u16 idx = (u16)mem_read16(mem, ring->base + RING_IDX_OFF);
  u64 entry = ring->base + RING_ENTRIES_OFF + (u64)(idx % ring->size) * USED_ENTRY_SIZE;
  mem_write32(mem, entry, id);
  mem_write32(mem, entry + 4, len);
  mem_write16(mem, ring->base + RING_IDX_OFF, (u16)(idx + 1));
}

// ---------------------------------------------------------------------------
// Transport helpers
// ---------------------------------------------------------------------------

void virtio_notify_guest(VirtioSlot *slot) {
  slot->interrupt_status |= 1;
  plic_set_pending(slot->irq);
}

// ---------------------------------------------------------------------------
// MMIO read
// ---------------------------------------------------------------------------

static VirtioSlot *slot_from_region(MemRegion *r) {
  return (VirtioSlot *)r->opaque;
}

static u64 virtio_read(MemRegion *r, u64 offset, size_t width) {
  (void)width;
  VirtioSlot *s = slot_from_region(r);

  switch (offset) {
  case VIRTIO_MMIO_MAGIC_OFF:
    return VIRTIO_MAGIC;
  case VIRTIO_MMIO_VERSION_OFF:
    return VIRTIO_VER;
  case VIRTIO_MMIO_DEVICE_ID_OFF:
    return s->dev->device_id;
  case VIRTIO_MMIO_VENDOR_ID_OFF:
    return VIRTIO_VENDOR_ID;
  case VIRTIO_MMIO_DEVICE_FEATURES_OFF:
    return (s->device_features_sel == 1) ? 1 : 0; // page 1, bit 0 = VERSION_1
  case VIRTIO_MMIO_QUEUE_NUM_MAX_OFF:
    return VIRTQUEUE_MAX_SIZE;
  case VIRTIO_MMIO_QUEUE_READY_OFF:
    return s->vq.ready ? 1 : 0;
  case VIRTIO_MMIO_INTERRUPT_STATUS_OFF:
    return s->interrupt_status;
  case VIRTIO_MMIO_STATUS_OFF:
    return s->status;
  default:
    break;
  }

  if (offset >= VIRTIO_MMIO_CONFIG_OFF && s->dev->read_config)
    return s->dev->read_config((u32)(offset - VIRTIO_MMIO_CONFIG_OFF));

  return 0;
}

// ---------------------------------------------------------------------------
// MMIO write
// ---------------------------------------------------------------------------

static void set_addr_low(u64 *addr, u64 val) {
  *addr = (*addr & 0xFFFFFFFF00000000ULL) | (u32)val;
}

static void set_addr_high(u64 *addr, u64 val) {
  *addr = (*addr & 0xFFFFFFFF) | (val << 32);
}

static void virtio_set_queue_size(VirtioSlot *s, u32 val) {
  s->vq.avail.size = (u16)val;
  s->vq.used.size = (u16)val;
}

static void virtio_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  (void)width;
  VirtioSlot *s = slot_from_region(r);

  switch (offset) {
  case VIRTIO_MMIO_DEVICE_FEATURES_SEL_OFF:
    s->device_features_sel = (u32)val;
    break;
  case VIRTIO_MMIO_DRIVER_FEATURES_OFF:
    break;
  case VIRTIO_MMIO_DRIVER_FEATURES_SEL_OFF:
    s->driver_features_sel = (u32)val;
    break;
  case VIRTIO_MMIO_QUEUE_SEL_OFF:
    s->queue_sel = (u32)val;
    break;
  case VIRTIO_MMIO_QUEUE_NUM_OFF:
    if (s->queue_sel == 0)
      virtio_set_queue_size(s, (u32)val);
    break;
  case VIRTIO_MMIO_QUEUE_READY_OFF:
    if (s->queue_sel == 0)
      s->vq.ready = (val != 0);
    break;
  case VIRTIO_MMIO_QUEUE_NOTIFY_OFF:
    if (val == 0 && s->dev->notify)
      s->dev->notify(s);
    break;
  case VIRTIO_MMIO_INTERRUPT_ACK_OFF:
    s->interrupt_status &= ~(u32)val;
    break;
  case VIRTIO_MMIO_STATUS_OFF:
    s->status = (u32)val;
    if (val == 0) {
      memset(&s->vq, 0, sizeof(s->vq));
      s->interrupt_status = 0;
    }
    break;
  case VIRTIO_MMIO_QUEUE_DESC_LOW_OFF:
    set_addr_low(&s->vq.desc_base, val);
    break;
  case VIRTIO_MMIO_QUEUE_DESC_HIGH_OFF:
    set_addr_high(&s->vq.desc_base, val);
    break;
  case VIRTIO_MMIO_QUEUE_DRIVER_LOW_OFF:
    set_addr_low(&s->vq.avail.base, val);
    break;
  case VIRTIO_MMIO_QUEUE_DRIVER_HIGH_OFF:
    set_addr_high(&s->vq.avail.base, val);
    break;
  case VIRTIO_MMIO_QUEUE_DEVICE_LOW_OFF:
    set_addr_low(&s->vq.used.base, val);
    break;
  case VIRTIO_MMIO_QUEUE_DEVICE_HIGH_OFF:
    set_addr_high(&s->vq.used.base, val);
    break;
  default:
    break;
  }
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void virtio_init(Memory *mem, const VirtioDevice *dev) {
  if (s_slot_count >= MAX_VIRTIO_DEVICES) {
    fprintf(stderr, "virtio: max devices (%d) reached\n", MAX_VIRTIO_DEVICES);
    return;
  }

  int idx = s_slot_count++;
  VirtioSlot *s = &s_slots[idx];
  s->mem = mem;
  s->dev = dev;
  s->irq = VIRTIO_IRQ + (u32)idx;

  u64 base = VIRTIO_MMIO_BASE + (u64)idx * VIRTIO_MMIO_SIZE;
  mem_add_device(mem, base, VIRTIO_MMIO_SIZE, virtio_read, virtio_write, s);
}