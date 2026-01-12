# Allocator Test Harness - Usage Guide

## Quick Start

```bash
# Build the test harness
make -f Makefile.test

# Run all tests with random seed
./test_allocator

# Run with specific seed for reproducibility
./test_allocator 12345

# Run only the original test suite
make -f Makefile.test test

# Run performance benchmark
make -f Makefile.test benchmark
```

## Integration with Your Allocator

Your allocator must implement these functions:

```c
void* my_malloc(size_t size);
void my_free(void* ptr);
void* my_realloc(void* ptr, size_t size);  // optional
void* my_calloc(size_t nmemb, size_t size);  // optional
```

## Example: Testing OpenAlloc

```c
#include "openalloc.h"
#include "test_allocator.h"

int main(int argc, char** argv) {
    // Initialize your allocator
    unsigned char heap[1024 * 1024];
    openalloc_init(heap, sizeof(heap));
    
    // Create interface
    allocator_interface_t iface = {
        .malloc = openalloc_malloc,
        .free = openalloc_free,
        .realloc = openalloc_realloc,
        .calloc = NULL  // Not implemented
    };
    
    // Set interface and run tests
    test_allocator_set_interface(&iface);
    test_allocator_run_all(12345);  // Fixed seed for reproducibility
    test_allocator_run_benchmark(12345);
    
    return 0;
}
```

## Example: Comparison with glibc malloc

```c
#include "test_allocator.h"
#include <stdlib.h>

int main(void) {
    // Test with system malloc
    allocator_interface_t glibc_iface = {
        .malloc = malloc,
        .free = free,
        .realloc = realloc,
        .calloc = calloc
    };
    
    printf("Testing glibc malloc:\n");
    test_allocator_set_interface(&glibc_iface);
    test_allocator_run_all(12345);
    test_allocator_run_benchmark(12345);
    
    return 0;
}
```

## Test Categories

### Basic Correctness
- malloc/free cycles
- adjacent allocations
- zero-size allocation
- NULL pointer handling
- alignment verification

### Boundary Conditions
- power-of-2 sizes (1B to 1MB)
- allocation storms (10,000 allocations)
- worst-case coalescing patterns
- partial fills (interleaved alloc/free)

### Multithreading
- 16 simultaneous threads
- lock contention testing
- race condition detection
- 100,000 operations per thread

### Fragmentation Resistance
- FIFO pattern (first-in-first-out)
- LIFO pattern (last-in-first-out)
- Random pattern
- 10,000 allocations/deallocations

### Performance Metrics
- Malloc operations per second
- Free operations per second
- Throughput (MB/sec)
- Peak RSS usage
- Average allocation time

### Memory Safety
- Double-free detection
- Use-after-free detection
- Buffer overrun detection
- Memory exhaustion handling

## Output Format

```
=== Running Test Suite (seed=12345) ===
  [PASS] basic_correctness
  [PASS] adjacent_allocations
  [PASS] alignment
  [SKIP] realloc not implemented
  [SKIP] calloc not implemented
  [PASS] double_free_detection
  [PASS] allocation_storm
  [PASS] power_of_2_sizes
  [PASS] worst_case_coalescing
  [PASS] partial_fills
  [PASS] multithreading
  [PASS] fifo_pattern
  [PASS] lifo_pattern
  [PASS] random_pattern

=== Test Summary ===
  Passed:  11
  Failed:  0
  Total:   11

=== Performance Benchmark ===
  Malloc ops/sec:  1234567
  Free ops/sec:    2345678
  Throughput:      1234.56 MB/sec
  Peak RSS:        1024 KB
```

## Reproducing Failures

If a test fails, run with the same seed:
```bash
./test_allocator <seed-from-failure>
```

The test harness captures signals and provides detailed failure information including:
- Test name
- Assertion location (file:line)
- Condition that failed

## Performance Targets

- Runtime: <1 second for full test suite
- Peak RSS: <100 MB
- Benchmark: <30 seconds for 1M operations

## Cross-Platform Compatibility

- Linux: Fully supported
- macOS: Fully supported
- Other POSIX: Should work with minor modifications

The harness uses standard C library features:
- pthread.h for multithreading
- sys/resource.h for memory tracking
- sys/time.h for high-resolution timing

## Advanced Usage

### Custom Test Suite

```c
void test_my_feature(void) {
    TEST_START("my_feature");
    
    void* ptr = current_allocator->malloc(1024);
    ASSERT_NOT_NULL(ptr);
    ASSERT_ALIGNED(ptr);
    
    current_allocator->free(ptr);
    
    TEST_PASS("my_feature");
}

int main(void) {
    test_allocator_set_interface(&my_allocator);
    
    // Run specific tests
    test_my_feature();
    
    return 0;
}
```

### Memory Leak Detection

The allocation tracker can be extended:
```c
static void verify_no_leaks(allocation_tracker_t* tracker) {
    ASSERT_EQ(tracker->count, 0);
}
```

## Troubleshooting

**Tests hang**: Check allocator locking in multithreading tests
**Segmentation faults**: Verify allocator handles edge cases (NULL, zero-size)
**High fragmentation**: Review coalescing implementation
**Poor performance**: Check lock contention and block splitting logic

## License

This test harness is provided as-is for testing custom memory allocators.
