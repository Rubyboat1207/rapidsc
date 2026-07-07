#include "arena.h"
#include <stdlib.h>
#include <string.h>

Arena_t* arenaNew(size_t size) {
    Arena_t* arena = malloc(sizeof(Arena_t));

    arena->mem = calloc(sizeof(uint8_t), size);
    arena->arenaSize = size;
    arena->bytesUsed = 0;

    return arena;
}

// was partially made using AI
void* arenaAllocate(Arena_t *arena, size_t size, size_t alignment) {
    uintptr_t base = (uintptr_t)arena->mem;
    uintptr_t current = base + arena->bytesUsed;

    uintptr_t aligned = (current + (alignment - 1)) & ~(uintptr_t)(alignment - 1);

    size_t padding = aligned - current;

    if (arena->bytesUsed + padding + size > arena->arenaSize) {
        return NULL;
    }

    void* data = (void*)aligned;
    arena->bytesUsed += padding + size;
    return data;
}

void arenaClear(Arena_t *arena) {
    memset(arena->mem, 0, arena->arenaSize);
    arena->bytesUsed = 0;
}
