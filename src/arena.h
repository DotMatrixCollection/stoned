#ifndef STONED_ARENA_H
#define STONED_ARENA_H

#include <stddef.h>

#define ARENA_BLOCK_SIZE (1024 * 64)

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t used;
    size_t capacity;
    char data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
} Arena;

Arena  arena_new(void);
void  *arena_alloc(Arena *a, size_t size);
void   arena_free(Arena *a);

#endif
