#include <stdlib.h>
#include <stdio.h>
#include "memory.h"

struct Memory {
  u8    *data;
  size_t size;
};

Memory *mem_create(size_t size) {
  Memory *mem = malloc(sizeof(Memory));
  if (!mem)
    return NULL;
  mem->data = calloc(1, size);
  if (!mem->data) {
    free(mem);
    return NULL;
  }
  mem->size = size;
  return mem;
}

void mem_destroy(Memory *mem) {
  if (!mem)
    return;
  free(mem->data);
  free(mem);
}

static void mem_check_bounds(const Memory *mem, u64 addr, size_t width) {
  if (addr + width > mem->size) {
    fprintf(stderr, "memory out of bounds: addr=0x%llx width=%zu\n", addr, width);
    exit(1);
  }
}

u8 mem_read8(const Memory *mem, u64 addr) {
  mem_check_bounds(mem, addr, 1);
  return *(u8 *)(mem->data + addr);
}
u16 mem_read16(const Memory *mem, u64 addr) {
  mem_check_bounds(mem, addr, 2);
  return *(u16 *)(mem->data + addr);
}
u32 mem_read32(const Memory *mem, u64 addr) {
  mem_check_bounds(mem, addr, 4);
  return *(u32 *)(mem->data + addr);
}
u64 mem_read64(const Memory *mem, u64 addr) {
  mem_check_bounds(mem, addr, 8);
  return *(u64 *)(mem->data + addr);
}

void mem_write8(const Memory *mem, u64 addr, u8 val) {
  mem_check_bounds(mem, addr, 1);
  *(u8 *)(mem->data + addr) = val;
}
void mem_write16(const Memory *mem, u64 addr, u16 val) {
  mem_check_bounds(mem, addr, 2);
  *(u16 *)(mem->data + addr) = val;
}
void mem_write32(const Memory *mem, u64 addr, u32 val) {
  mem_check_bounds(mem, addr, 4);
  *(u32 *)(mem->data + addr) = val;
}
void mem_write64(const Memory *mem, u64 addr, u64 val) {
  mem_check_bounds(mem, addr, 8);
  *(u64 *)(mem->data + addr) = val;
}

bool mem_load_file(Memory *mem, const char *path, u64 offset) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "mem_load_file: cannot open '%s'\n", path);
    return false;
  }

  fseek(f, 0, SEEK_END);
  size_t file_size = ftell(f);
  rewind(f);

  if (offset + file_size > mem->size) {
    fprintf(stderr, "mem_load_file: file too large: offset=0x%llx size=%zu\n", offset, file_size);
    fclose(f);
    return false;
  }

  fread(mem->data + offset, 1, file_size, f);
  fclose(f);
  return true;
}