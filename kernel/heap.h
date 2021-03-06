#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "locks.h"

/* 
 * TODO: We can get rid of the next pointer, 
 * just use current address + size
 */
struct heap_block_header {
    unsigned magic;
    unsigned flags;         /* Allocated/free */
    unsigned size;          /* Including the header */
    struct heap_block_header* next;
};

/*
 * TODO: We can get rid of the head pointer
 * Just use address + sizeof(struct heap)
 */
struct heap {
    struct heap_block_header* head;
    unsigned size;          /* Including this header */
    unsigned max_size;
    spinlock_t lock;
};

struct heap_info {
    void* address;
    unsigned size;
    unsigned free;
};

struct heap* heap_init(void* address, unsigned size, unsigned max_size);
struct heap_info heap_info(struct heap* heap);
struct heap_block_header* heap_alloc_block_aligned(struct heap* heap, unsigned size, unsigned alignment);
struct heap_block_header* heap_alloc_block(struct heap* heap, unsigned size);
void heap_free_block(struct heap* heap, struct heap_block_header* block);
void heap_dump(struct heap* heap);
bool heap_lock(struct heap* heap);
bool heap_unlock(struct heap* heap);
void heap_check(struct heap* heap, const char* file, int line);
void* heap_alloc(struct heap*, unsigned);
void* heap_alloc_aligned(struct heap*, unsigned, unsigned);
void heap_free(struct heap*, void*);


