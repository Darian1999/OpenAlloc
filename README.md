# OpenAlloc - Fast Memory Allocator

A high-performance, single-file memory allocator for C with hardened security features.

## Features

- **Segregated Free Lists** (default) - ~3x faster than glibc malloc
- **Hardened Security** - Double-free detection, memory poisoning, header corruption checks
- **Simplified API** - Just `init`, `malloc`, `free`, `realloc`
- **No External Dependencies** - Pure C standard library
- **Single-Threaded** - Fast, predictable, no locks
- **Configurable** - Build with or without segregation

## Quick Start

```bash
# Build (default: fast segregated version)
make

# Run tests
make run-test

# Build without segregation (original version with coalescing)
make no-seg

# Build and run tests (non-segregated)
make no-seg run-test
```

## Usage

```c
#include "openalloc.h"

#define HEAP_SIZE (1024 * 1024)
static unsigned char heap[HEAP_SIZE];

int main(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr = openalloc_malloc(100);
    openalloc_free(ptr);
    
    return 0;
}
```

## Performance

### Segregated Version (Default)

| Operation | Speed | vs glibc |
|-----------|-------|-----------|
| Malloc    | 3.43M ops/sec (292ns) | **Faster** |
| Free      | 21.5M ops/sec (47ns) | **4.4x faster** |
| Total     | 25M ops/sec | **3x faster** |

### Original Version (--no-seg)

| Operation | Speed | vs glibc |
|-----------|-------|-----------|
| Malloc    | ~1.2M ops/sec (861ns) | Slower |
| Free      | 19.2M ops/sec (52ns) | Faster |

## Build Options

```bash
make                  # Segregated free list (fast)
make no-seg            # Original with coalescing (slow)
make run-test           # Run tests (current build)
make run-benchmark       # Run benchmark (current build)
```

## Security Features (Hardened Version)

The hardened version includes critical security protections:

1. **Double-Free Detection** - Aborts on attempt to free the same block twice
2. **Memory Poisoning** - Freed blocks are filled with `0x5A` to detect use-after-free
3. **Header Corruption Detection** - Magic number (`0xDEADBEEF`) in block headers prevents use of corrupted/misallocated pointers

These security features add minimal overhead (4 bytes per block) while preventing common memory safety vulnerabilities.

## Architecture

### Segregated Allocator (Default)

```
Bin 0: 0-64 bytes
Bin 1: 65-128 bytes
Bin 2: 129-256 bytes
Bin 3: 257-512 bytes
Bin 4: 513-1024 bytes
Bin 5: 1025-2048 bytes
Bin 6: 2049-4096 bytes
Bin 7: 4097-8192 bytes
Bin 8: 8193-16384 bytes
Bin 9: >16384 bytes
```

Each bin maintains a singly-linked free list. Allocation searches from the smallest bin that can satisfy the request.

### Block Header

```c
typedef struct block_header {
    size_t size;           // Block size (excluding header)
    struct block_header* next; // Next block in free list
    uint8_t free;           // Is block free? (0=no, 1=yes)
    uint32_t magic;         // Magic number for corruption detection (0xDEADBEEF)
} block_header_t;
```

## API

```c
int openalloc_init(void* heap_ptr, size_t size);
void* openalloc_malloc(size_t size);
void openalloc_free(void* ptr);
void* openalloc_realloc(void* ptr, size_t new_size);
size_t openalloc_usable_size(void* ptr);
void openalloc_get_stats(openalloc_stats_t* stats);
```

## Testing

```bash
# Original test suite
make run-test

# Comprehensive test harness
make -f Makefile.test run-test-allocator
```

## See Also

- `SEGRAGATION_SUMMARY.md` - Performance comparison and architecture details
- `test_allocator.h` - Test harness API
- `TEST_README.md` - Test harness documentation

## License

MIT License - Feel free to use in any project.
