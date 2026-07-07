#ifndef ARENA
#define ARENA
#include <stddef.h>
#include <stdint.h>

// This intentionally does not deal with alignment because frankly I couldn't care less
typedef struct {
    void* mem;
    size_t bytesUsed;
    size_t arenaSize;
} Arena_t;

Arena_t* arenaNew(size_t size);
void* arenaAllocate(Arena_t *arena, size_t size, size_t alignment);
void arenaClear(Arena_t *arena);
#endif
