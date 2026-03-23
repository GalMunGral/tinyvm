#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

#define MAX_REGIONS 8

struct MemRegion {
  u64        base;
  size_t     size;
  u8        *data;
  MemReadFn  read;
  MemWriteFn write;
};

struct Memory {
  MemRegion regions[MAX_REGIONS];
  int       count;
};

// ---------------------------------------------------------------------------
// Default handlers — plain byte-array read/write
// ---------------------------------------------------------------------------
static u64 default_read(MemRegion *r, u64 offset, size_t width) {
  u64 val = 0;
  memcpy(&val, r->data + offset, width);
  return val;
}

static void default_write(MemRegion *r, u64 offset, u64 val, size_t width) {
  memcpy(r->data + offset, &val, width);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
static MemRegion *mem_find_region(const Memory *mem, u64 addr, size_t width) {
  for (int i = 0; i < mem->count; i++) {
    MemRegion *r = (MemRegion *)&mem->regions[i];
    if (addr >= r->base && addr + width <= r->base + r->size)
      return r;
  }
  fprintf(stderr, "memory out of bounds: addr=0x%llx width=%zu\n", addr, width);
  exit(1);
}

static bool add_region(Memory *mem, u64 base, size_t size, u8 *data, MemReadFn read,
                       MemWriteFn write) {
  if (mem->count >= MAX_REGIONS) {
    fprintf(stderr, "add_region: max regions (%d) reached\n", MAX_REGIONS);
    return false;
  }
  mem->regions[mem->count++] =
      (MemRegion){.base = base, .size = size, .data = data, .read = read, .write = write};
  return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
Memory *mem_create(void) { return calloc(1, sizeof(Memory)); }

void mem_destroy(Memory *mem) {
  if (!mem)
    return;
  for (int i = 0; i < mem->count; i++)
    free(mem->regions[i].data);
  free(mem);
}

bool mem_add_region(Memory *mem, u64 base, size_t size) {
  u8 *data = calloc(1, size);
  if (!data)
    return false;
  return add_region(mem, base, size, data, default_read, default_write);
}

bool mem_add_device(Memory *mem, u64 base, size_t size, MemReadFn read, MemWriteFn write) {
  return add_region(mem, base, size, NULL, read, write);
}

u8 mem_read8(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 1);
  return (u8)r->read(r, addr - r->base, 1);
}
u16 mem_read16(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 2);
  return (u16)r->read(r, addr - r->base, 2);
}
u32 mem_read32(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 4);
  return (u32)r->read(r, addr - r->base, 4);
}
u64 mem_read64(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 8);
  return r->read(r, addr - r->base, 8);
}

void mem_write8(const Memory *mem, u64 addr, u8 val) {
  MemRegion *r = mem_find_region(mem, addr, 1);
  r->write(r, addr - r->base, val, 1);
}
void mem_write16(const Memory *mem, u64 addr, u16 val) {
  MemRegion *r = mem_find_region(mem, addr, 2);
  r->write(r, addr - r->base, val, 2);
}
void mem_write32(const Memory *mem, u64 addr, u32 val) {
  MemRegion *r = mem_find_region(mem, addr, 4);
  r->write(r, addr - r->base, val, 4);
}
void mem_write64(const Memory *mem, u64 addr, u64 val) {
  MemRegion *r = mem_find_region(mem, addr, 8);
  r->write(r, addr - r->base, val, 8);
}

void mem_read_buf(const Memory *mem, u64 addr, void *dst, size_t len) {
  MemRegion *r = mem_find_region(mem, addr, len);
  memcpy(dst, r->data + (addr - r->base), len);
}

void mem_write_buf(const Memory *mem, u64 addr, const void *src, size_t len) {
  MemRegion *r = mem_find_region(mem, addr, len);
  memcpy(r->data + (addr - r->base), src, len);
}

bool mem_load_file(Memory *mem, const char *path, u64 addr) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "mem_load_file: cannot open '%s'\n", path);
    return false;
  }

  fseek(f, 0, SEEK_END);
  size_t file_size = ftell(f);
  rewind(f);

  MemRegion *r = mem_find_region(mem, addr, file_size);
  fread(r->data + (addr - r->base), 1, file_size, f);
  fclose(f);
  return true;
}
