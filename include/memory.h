#pragma once

#include "types.h"

#define MAX_REGIONS 8

typedef struct MemRegion MemRegion;

typedef u64 (*MemReadFn)(MemRegion *r, u64 offset, size_t width);
typedef void (*MemWriteFn)(MemRegion *r, u64 offset, u64 val, size_t width);

struct MemRegion {
  u64        base;
  size_t     size;
  u8        *data;
  MemReadFn  read;
  MemWriteFn write;
};

typedef struct {
  MemRegion regions[MAX_REGIONS];
  int       count;
} Memory;

bool mem_add_region(Memory *mem, u64 base, size_t size);
bool mem_add_device(Memory *mem, u64 base, size_t size, MemReadFn read, MemWriteFn write);

u8  mem_read8(const Memory *mem, u64 addr);
u16 mem_read16(const Memory *mem, u64 addr);
u32 mem_read32(const Memory *mem, u64 addr);
u64 mem_read64(const Memory *mem, u64 addr);

void mem_write8(const Memory *mem, u64 addr, u8 val);
void mem_write16(const Memory *mem, u64 addr, u16 val);
void mem_write32(const Memory *mem, u64 addr, u32 val);
void mem_write64(const Memory *mem, u64 addr, u64 val);

bool mem_load_file(Memory *mem, const char *path, u64 addr);
void mem_read_buf(const Memory *mem, u64 addr, void *dst, size_t len);
void mem_write_buf(const Memory *mem, u64 addr, const void *src, size_t len);