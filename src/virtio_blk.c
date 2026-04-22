#include "virtio.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SECTOR_SIZE 512
#define DEFAULT_DISK_SIZE (64 * 1024 * 1024) // 64MB

// Block request types
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_GET_ID 8

// Block request status
#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1

static struct {
  int disk_fd;
  u64 disk_capacity; // in sectors
} s_blk;

// ---------------------------------------------------------------------------
// I/O helpers
// ---------------------------------------------------------------------------

static u8 blk_do_read(off_t offset, u64 dst, u32 len, const Memory *mem) {
  u8 buf[SECTOR_SIZE];
  while (len > 0) {
    u32 chunk = len < SECTOR_SIZE ? len : SECTOR_SIZE;
    ssize_t n = pread(s_blk.disk_fd, buf, chunk, offset);
    if (n < 0)
      return VIRTIO_BLK_S_IOERR;
    mem_write_buf(mem, dst, buf, (size_t)n);
    dst += (u64)n;
    offset += n;
    len -= (u32)n;
  }
  return VIRTIO_BLK_S_OK;
}

static u8 blk_do_write(off_t offset, u64 src, u32 len, const Memory *mem) {
  u8 buf[SECTOR_SIZE];
  while (len > 0) {
    u32 chunk = len < SECTOR_SIZE ? len : SECTOR_SIZE;
    mem_read_buf(mem, src, buf, chunk);
    ssize_t n = pwrite(s_blk.disk_fd, buf, chunk, offset);
    if (n < 0)
      return VIRTIO_BLK_S_IOERR;
    src += (u64)n;
    offset += n;
    len -= (u32)n;
  }
  return VIRTIO_BLK_S_OK;
}

static u8 blk_do_get_id(u64 dst, u32 len, const Memory *mem) {
  const char *id = "tinyvm-blk";
  size_t id_len = strlen(id) + 1;
  if (id_len > len)
    id_len = len;
  mem_write_buf(mem, dst, id, id_len);
  return VIRTIO_BLK_S_OK;
}

// ---------------------------------------------------------------------------
// Request handling
// ---------------------------------------------------------------------------

static void blk_handle_request(const Memory *mem, Virtqueue *vq, u16 head) {
  VirtqDesc hdr    = vq_desc_read(mem, vq->desc_base, head);
  VirtqDesc data   = vq_desc_read(mem, vq->desc_base, hdr.next);
  VirtqDesc status = vq_desc_read(mem, vq->desc_base, data.next);

  u32 type   = mem_read32(mem, hdr.addr);
  u64 sector = mem_read64(mem, hdr.addr + 8);
  off_t offset = (off_t)(sector * SECTOR_SIZE);

  u8 result;
  switch (type) {
  case VIRTIO_BLK_T_IN:
    result = blk_do_read(offset, data.addr, data.len, mem);
    break;
  case VIRTIO_BLK_T_OUT:
    result = blk_do_write(offset, data.addr, data.len, mem);
    break;
  case VIRTIO_BLK_T_GET_ID:
    result = blk_do_get_id(data.addr, data.len, mem);
    break;
  default:
    result = VIRTIO_BLK_S_IOERR;
    break;
  }

  mem_write8(mem, status.addr, result);
  vq_ring_post(mem, &vq->used, head, 0);
}

static void blk_notify(VirtioSlot *slot) {
  Virtqueue *vq = &slot->vq;
  const Memory *mem = slot->mem;
  if (!vq->ready)
    return;

  while (vq_ring_has_next(mem, &vq->avail)) {
    u16 head = vq_ring_next(mem, &vq->avail);
    blk_handle_request(mem, vq, head);
  }

  virtio_notify_guest(slot);
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

static u64 blk_read_config(u32 offset) {
  if (offset == 0)
    return (u32)(s_blk.disk_capacity);
  if (offset == 4)
    return (u32)(s_blk.disk_capacity >> 32);
  return 0;
}

static const VirtioDevice s_blk_dev = {
    .device_id   = 2,
    .notify      = blk_notify,
    .read_config = blk_read_config,
};

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void virtio_blk_init(Memory *mem, CPU *cpu, const char *disk_path) {
  (void)cpu;

  s_blk.disk_fd = open(disk_path, O_RDWR | O_CREAT, 0644);
  if (s_blk.disk_fd < 0) {
    fprintf(stderr, "virtio_blk: cannot open '%s'\n", disk_path);
    return;
  }

  struct stat st;
  fstat(s_blk.disk_fd, &st);
  if (st.st_size == 0) {
    ftruncate(s_blk.disk_fd, DEFAULT_DISK_SIZE);
    st.st_size = DEFAULT_DISK_SIZE;
  }
  s_blk.disk_capacity = (u64)st.st_size / SECTOR_SIZE;

  virtio_init(mem, &s_blk_dev);
}