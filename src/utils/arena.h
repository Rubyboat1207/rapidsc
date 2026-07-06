#include <stdint.h>

// This intentionally does not deal with alignment because frankly I couldn't care less
typedef struct {
    void* arena;
    uint32_t bytesUsed;
    uint32_t arenaSize;
} Arena_t;

void* arenaAllocate(uint32_t size);
