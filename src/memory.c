#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

#define MAX_REGIONS 8

typedef struct {
  u64    base;
  size_t size;
  u8    *data;
} MemRegion;

struct Memory {
  MemRegion regions[MAX_REGIONS];
  int       count;
};

Memory *mem_create(void) { return calloc(1, sizeof(Memory)); }

void mem_destroy(Memory *mem) {
  if (!mem)
    return;
  for (int i = 0; i < mem->count; i++)
    free(mem->regions[i].data);
  free(mem);
}

bool mem_add_region(Memory *mem, u64 base, size_t size) {
  if (mem->count >= MAX_REGIONS) {
    fprintf(stderr, "mem_add_region: max regions (%d) reached\n", MAX_REGIONS);
    return false;
  }
  u8 *data = calloc(1, size);
  if (!data)
    return false;
  mem->regions[mem->count++] = (MemRegion){.base = base, .size = size, .data = data};
  return true;
}

static MemRegion *mem_find_region(const Memory *mem, u64 addr, size_t width) {
  for (int i = 0; i < mem->count; i++) {
    MemRegion *r = (MemRegion *)&mem->regions[i];
    if (addr >= r->base && addr + width <= r->base + r->size)
      return r;
  }
  fprintf(stderr, "memory out of bounds: addr=0x%llx width=%zu\n", addr, width);
  exit(1);
}

u8 mem_read8(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 1);
  return *(u8 *)(r->data + (addr - r->base));
}
u16 mem_read16(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 2);
  return *(u16 *)(r->data + (addr - r->base));
}
u32 mem_read32(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 4);
  return *(u32 *)(r->data + (addr - r->base));
}
u64 mem_read64(const Memory *mem, u64 addr) {
  MemRegion *r = mem_find_region(mem, addr, 8);
  return *(u64 *)(r->data + (addr - r->base));
}

void mem_write8(const Memory *mem, u64 addr, u8 val) {
  MemRegion *r                        = mem_find_region(mem, addr, 1);
  *(u8 *)(r->data + (addr - r->base)) = val;
}
void mem_write16(const Memory *mem, u64 addr, u16 val) {
  MemRegion *r                         = mem_find_region(mem, addr, 2);
  *(u16 *)(r->data + (addr - r->base)) = val;
}
void mem_write32(const Memory *mem, u64 addr, u32 val) {
  MemRegion *r                         = mem_find_region(mem, addr, 4);
  *(u32 *)(r->data + (addr - r->base)) = val;
}
void mem_write64(const Memory *mem, u64 addr, u64 val) {
  MemRegion *r                         = mem_find_region(mem, addr, 8);
  *(u64 *)(r->data + (addr - r->base)) = val;
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