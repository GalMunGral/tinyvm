#pragma once

#include "cpu.h"
#include "memory.h"

#define VIRTIO_MMIO_BASE 0x10001000ULL
#define VIRTIO_MMIO_SIZE 0x1000ULL
#define VIRTIO_IRQ 2 // first PLIC source (slots get IRQ 2, 3, 4, ...)

// MMIO register offsets (virtio spec 4.2.2)
#define VIRTIO_MMIO_MAGIC_OFF 0x000
#define VIRTIO_MMIO_VERSION_OFF 0x004
#define VIRTIO_MMIO_DEVICE_ID_OFF 0x008
#define VIRTIO_MMIO_VENDOR_ID_OFF 0x00C
#define VIRTIO_MMIO_DEVICE_FEATURES_OFF 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL_OFF 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES_OFF 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL_OFF 0x024
#define VIRTIO_MMIO_QUEUE_SEL_OFF 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX_OFF 0x034
#define VIRTIO_MMIO_QUEUE_NUM_OFF 0x038
#define VIRTIO_MMIO_QUEUE_READY_OFF 0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY_OFF 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS_OFF 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK_OFF 0x064
#define VIRTIO_MMIO_STATUS_OFF 0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW_OFF 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH_OFF 0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW_OFF 0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH_OFF 0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW_OFF 0x0A0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH_OFF 0x0A4
#define VIRTIO_MMIO_CONFIG_OFF 0x100

// Virtqueue descriptor flags
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

// Device status bits
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8

// Ring buffer — shared between available and used rings
typedef struct {
  u64 base;
  u16 size;
  u16 cursor; // host-side tracking index
} VirtqRing;

// Virtqueue state
typedef struct {
  u64       desc_base;
  VirtqRing avail;
  VirtqRing used;
  bool      ready;
} Virtqueue;

// Descriptor fields
typedef struct {
  u64 addr;
  u32 len;
  u16 flags;
  u16 next;
} VirtqDesc;

// Forward declaration
typedef struct VirtioSlot VirtioSlot;

// Device-specific callbacks (provided by e.g. virtio_blk)
typedef struct {
  u32 device_id;
  void (*notify)(VirtioSlot *slot);
  u64 (*read_config)(u32 offset);
} VirtioDevice;

// Transport state for one virtio-mmio slot
struct VirtioSlot {
  const VirtioDevice *dev;
  const Memory       *mem;
  Virtqueue           vq;
  u32                 status;
  u32                 device_features_sel;
  u32                 driver_features_sel;
  u32                 queue_sel;
  u32                 interrupt_status;
  u32                 irq;
};

// Transport API
void virtio_init(Memory *mem, const VirtioDevice *dev);
void virtio_notify_guest(VirtioSlot *slot);

// Descriptor table
VirtqDesc vq_desc_read(const Memory *mem, u64 desc_base, u16 index);

// Ring buffer operations
bool vq_ring_has_next(const Memory *mem, VirtqRing *ring);
u16  vq_ring_next(const Memory *mem, VirtqRing *ring);
void vq_ring_post(const Memory *mem, VirtqRing *ring, u16 id, u32 len);

// Block device
void virtio_blk_init(Memory *mem, CPU *cpu, const char *disk_path);
