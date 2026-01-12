#include "openalloc.h"
#include <string.h>

#define NUM_BINS 10

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define BLOCK_HEADER_SIZE 16

typedef struct block_header {
    size_t size;
    struct block_header* next;
    uint8_t free;
} block_header_t;

#ifdef OPENALLOC_NO_SEG

typedef struct {
    struct block_header* next;
    struct block_header* prev;
} doubly_linked_t;

typedef struct block_header_doubly {
    size_t size;
    struct block_header_doubly* next;
    struct block_header_doubly* prev;
    uint8_t free;
    uint8_t pad[3];
} block_header_doubly_t;

static block_header_doubly_t* free_list = NULL;
static void* heap_start = NULL;
static size_t heap_size = 0;

static size_t align_size(size_t size) {
    return (size + OPENALLOC_ALIGN - 1) & ~(OPENALLOC_ALIGN - 1);
}

static block_header_doubly_t* get_block_doubly(void* ptr) {
    return (block_header_doubly_t*)((uint8_t*)ptr - sizeof(block_header_doubly_t));
}

static void* get_data_doubly(block_header_doubly_t* block) {
    return (uint8_t*)block + sizeof(block_header_doubly_t);
}

static void split_block_doubly(block_header_doubly_t* block, size_t size) {
    if (block->size - size >= OPENALLOC_MIN_BLOCK + sizeof(block_header_doubly_t)) {
        block_header_doubly_t* new_block = (block_header_doubly_t*)((uint8_t*)block + sizeof(block_header_doubly_t) + size);
        new_block->size = block->size - size - sizeof(block_header_doubly_t);
        new_block->free = 1;
        new_block->next = NULL;
        new_block->prev = NULL;
        
        block->size = size;
        
        if (free_list) {
            new_block->next = free_list;
            free_list->prev = new_block;
        }
        free_list = new_block;
    }
}

static void coalesce_block_doubly(block_header_doubly_t* block) {
    if (!block->free) return;
    
    block_header_doubly_t* next = (block_header_doubly_t*)((uint8_t*)block + sizeof(block_header_doubly_t) + block->size);
    
    if ((uint8_t*)next < (uint8_t*)heap_start + heap_size && next->free) {
        if (next->prev) {
            next->prev->next = next->next;
        } else {
            free_list = next->next;
        }
        if (next->next) {
            next->next->prev = next->prev;
        }
        
        block->size += next->size + sizeof(block_header_doubly_t);
    }
    
    block_header_doubly_t* prev = NULL;
    block_header_doubly_t* curr = (block_header_doubly_t*)heap_start;
    while (curr < block) {
        block_header_doubly_t* temp = (block_header_doubly_t*)((uint8_t*)curr + sizeof(block_header_doubly_t) + curr->size);
        if (temp == block && curr->free) {
            prev = curr;
            break;
        }
        curr = temp;
    }
    
    if (prev) {
        if (prev->prev) {
            prev->prev->next = prev->next;
        } else {
            free_list = prev->next;
        }
        if (prev->next) {
            prev->next->prev = prev->prev;
        }
        
        prev->size += block->size + sizeof(block_header_doubly_t);
        block = prev;
    }
}

int openalloc_init(void* heap_ptr, size_t size) {
    if (!heap_ptr || size < OPENALLOC_MIN_BLOCK + sizeof(block_header_doubly_t)) {
        return -1;
    }
    
    heap_start = heap_ptr;
    heap_size = size;
    
    free_list = (block_header_doubly_t*)heap_start;
    free_list->size = size - sizeof(block_header_doubly_t);
    free_list->free = 1;
    free_list->next = NULL;
    free_list->prev = NULL;
    
    return 0;
}

void* openalloc_malloc(size_t size) {
    if (size == 0) return NULL;
    
    size_t aligned_size = align_size(size);
    
    block_header_doubly_t* block = free_list;
    while (block) {
        if (block->free && block->size >= aligned_size) {
            split_block_doubly(block, aligned_size);
            block->free = 0;
            
            if (block->prev) {
                block->prev->next = block->next;
            } else {
                free_list = block->next;
            }
            if (block->next) {
                block->next->prev = block->prev;
            }
            block->next = NULL;
            block->prev = NULL;
            
            return get_data_doubly(block);
        }
        block = block->next;
    }
    
    return NULL;
}

void openalloc_free(void* ptr) {
    if (!ptr) return;
    
    block_header_doubly_t* block = get_block_doubly(ptr);
    
    block->free = 1;
    coalesce_block_doubly(block);
    
    block->next = free_list;
    block->prev = NULL;
    if (free_list) {
        free_list->prev = block;
    }
    free_list = block;
}

#else

static block_header_t* free_lists[NUM_BINS] __attribute__((aligned(64))) = {NULL};
static void* heap_start = NULL;
static size_t heap_size = 0;

static inline size_t align_size(size_t size) {
    return (size + OPENALLOC_ALIGN - 1) & ~(OPENALLOC_ALIGN - 1);
}

static inline int get_bin(size_t size) {
    if (LIKELY(size <= 16)) return 0;
    if (LIKELY(size <= 32)) return 1;
    if (LIKELY(size <= 64)) return 2;
    if (LIKELY(size <= 128)) return 3;
    if (LIKELY(size <= 256)) return 4;
    if (LIKELY(size <= 512)) return 5;
    if (LIKELY(size <= 1024)) return 6;
    if (LIKELY(size <= 2048)) return 7;
    if (LIKELY(size <= 4096)) return 8;
    return 9;
}

static inline block_header_t* get_block(void* ptr) {
    return (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
}

static inline void* get_data(block_header_t* block) {
    return (uint8_t*)block + sizeof(block_header_t);
}

int openalloc_init(void* heap_ptr, size_t size) {
    if (!heap_ptr || size < OPENALLOC_MIN_BLOCK + sizeof(block_header_t)) {
        return -1;
    }
    
    heap_start = heap_ptr;
    heap_size = size;
    
    for (int i = 0; i < NUM_BINS; i++) {
        free_lists[i] = NULL;
    }
    
    block_header_t* block = (block_header_t*)heap_start;
    block->size = size - sizeof(block_header_t);
    block->free = 1;
    block->next = NULL;
    free_lists[NUM_BINS - 1] = block;
    
    return 0;
}

void* openalloc_malloc(size_t size) {
    if (UNLIKELY(size == 0)) return NULL;
    
    size_t aligned_size = align_size(size);
    int start_bin = get_bin(aligned_size);
    
    for (int bin = start_bin; bin < NUM_BINS; bin++) {
        block_header_t** prev = &free_lists[bin];
        block_header_t* block = *prev;
        
        while (LIKELY(block != NULL)) {
            if (LIKELY(block->free) && LIKELY(block->size >= aligned_size)) {
                block_header_t* original_next = block->next;
                
                if (LIKELY(block->size >= aligned_size + OPENALLOC_MIN_BLOCK + sizeof(block_header_t))) {
                    block_header_t* new_block = (block_header_t*)((uint8_t*)block + sizeof(block_header_t) + aligned_size);
                    new_block->size = block->size - aligned_size - sizeof(block_header_t);
                    new_block->free = 1;
                    new_block->next = original_next;
                    
                    block->size = aligned_size;
                    
                    int new_bin = get_bin(new_block->size);
                    
                    if (LIKELY(new_bin == bin)) {
                        new_block->next = block->next;
                        *prev = new_block;
                        prev = &new_block->next;
                    } else {
                        new_block->next = free_lists[new_bin];
                        free_lists[new_bin] = new_block;
                    }
                }
                
                block->free = 0;
                *prev = original_next;
                block->next = NULL;
                
                return get_data(block);
            }
            prev = &block->next;
            block = block->next;
        }
    }
    
    return NULL;
}

void openalloc_free(void* ptr) {
    if (UNLIKELY(!ptr)) return;
    
    block_header_t* block = get_block(ptr);
    block->free = 1;
    
    int bin = get_bin(block->size);
    block->next = free_lists[bin];
    free_lists[bin] = block;
}

#endif

void* openalloc_realloc(void* ptr, size_t new_size) {
    if (!ptr) return openalloc_malloc(new_size);
    if (new_size == 0) {
        openalloc_free(ptr);
        return NULL;
    }
    
#ifdef OPENALLOC_NO_SEG
    block_header_doubly_t* block = get_block_doubly(ptr);
    size_t old_size = block->size;
#else
    block_header_t* block = get_block(ptr);
    size_t old_size = block->size;
#endif
    
    if (new_size <= old_size) {
        return ptr;
    }
    
    void* new_ptr = openalloc_malloc(new_size);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, old_size);
    openalloc_free(ptr);
    
    return new_ptr;
}

size_t openalloc_usable_size(void* ptr) {
    if (!ptr) return 0;
#ifdef OPENALLOC_NO_SEG
    return get_block_doubly(ptr)->size;
#else
    return get_block(ptr)->size;
#endif
}

void openalloc_get_stats(openalloc_stats_t* stats) {
    if (!stats) return;
    
    stats->heap_start = heap_start;
    stats->heap_size = heap_size;
    stats->allocated_blocks = 0;
    stats->free_blocks = 0;
    stats->total_allocated = 0;
    stats->total_freed = 0;
    
#ifdef OPENALLOC_NO_SEG
    block_header_doubly_t* block = (block_header_doubly_t*)heap_start;
    size_t header_size = sizeof(block_header_doubly_t);
    while ((uint8_t*)block < (uint8_t*)heap_start + heap_size) {
        if (block->free) {
            stats->free_blocks++;
            stats->total_freed += block->size;
        } else {
            stats->allocated_blocks++;
            stats->total_allocated += block->size;
        }
        block = (block_header_doubly_t*)((uint8_t*)block + header_size + block->size);
    }
#else
    block_header_t* block = (block_header_t*)heap_start;
    size_t header_size = sizeof(block_header_t);
    while ((uint8_t*)block < (uint8_t*)heap_start + heap_size) {
        if (block->free) {
            stats->free_blocks++;
            stats->total_freed += block->size;
        } else {
            stats->allocated_blocks++;
            stats->total_allocated += block->size;
        }
        block = (block_header_t*)((uint8_t*)block + header_size + block->size);
    }
#endif
}
