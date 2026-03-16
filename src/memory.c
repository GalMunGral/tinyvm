#include <stdlib.h>
#include "memory.h"

struct Memory {
    u8     *data;
    size_t  size;
};

Memory *mem_create(size_t size) {
    return NULL;
}

void mem_destroy(Memory *mem) {
}

u8  mem_read8 (const Memory *mem, u64 addr) { return 0; }
u16 mem_read16(const Memory *mem, u64 addr) { return 0; }
u32 mem_read32(const Memory *mem, u64 addr) { return 0; }
u64 mem_read64(const Memory *mem, u64 addr) { return 0; }

void mem_write8 (Memory *mem, u64 addr, u8  val) {}
void mem_write16(Memory *mem, u64 addr, u16 val) {}
void mem_write32(Memory *mem, u64 addr, u32 val) {}
void mem_write64(Memory *mem, u64 addr, u64 val) {}

bool mem_load_file(Memory *mem, const char *path, u64 offset) {
    return false;
}