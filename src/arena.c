#include "arena.h"
#include <stdlib.h>
#include <string.h>

static ArenaBlock *block_new(size_t capacity) {
    ArenaBlock *b = malloc(sizeof(ArenaBlock) + capacity);
    if (!b) return NULL;
    b->next     = NULL;
    b->used     = 0;
    b->capacity = capacity;
    return b;
}

Arena arena_new(void) {
    Arena a;
    a.head = block_new(ARENA_BLOCK_SIZE);
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    /* align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    if (a->head->used + size > a->head->capacity) {
        size_t cap = size > ARENA_BLOCK_SIZE ? size : ARENA_BLOCK_SIZE;
        ArenaBlock *b = block_new(cap);
        b->next = a->head;
        a->head = b;
    }

    void *ptr = a->head->data + a->head->used;
    a->head->used += size;
    memset(ptr, 0, size);
    return ptr;
}

void arena_free(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
}
