#ifndef OPENALLOC_H
#define OPENALLOC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* heap_start;
    size_t heap_size;
    size_t allocated_blocks;
    size_t free_blocks;
    size_t total_allocated;
    size_t total_freed;
} openalloc_stats_t;

int openalloc_init(void* heap_start, size_t heap_size);
void* openalloc_malloc(size_t size);
void openalloc_free(void* ptr);
void* openalloc_realloc(void* ptr, size_t new_size);
size_t openalloc_usable_size(void* ptr);
void openalloc_get_stats(openalloc_stats_t* stats);

#define OPENALLOC_ALIGN 8
#define OPENALLOC_MIN_BLOCK (sizeof(size_t) * 2)

#define OPENALLOC_MAGIC 0x3AB640B4
#define OPENALLOC_POISON 0x5A

#ifdef __cplusplus
}
#endif

#endif
