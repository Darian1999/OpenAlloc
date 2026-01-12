# Allocator Test Harness

A lightweight, stable test system for validating custom C memory allocators.

## Features

- **Lightweight**: Single-file implementation, no external dependencies, compiles with `-O2 -Wall -std=c11`
- **Stable**: Deterministic output with seedable RNG, reproducible failures
- **Comprehensive**: Covers correctness, boundary conditions, fragmentation, and performance

## Quick Start

```bash
# Build the test harness
make -f Makefile.test

# Run comprehensive test suite (with random seed)
./test_allocator

# Run with specific seed for reproducibility
./test_allocator 12345

# Run original test suite
make -f Makefile.test run-test

# Run performance benchmark
make -f Makefile.test benchmark
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
- Skipped by default (requires thread-safe allocator)
- 16 simultaneous threads
- 100,000 operations per thread

### Fragmentation Resistance
- FIFO pattern (first-in-first-out)
- LIFO pattern (last-in-first-out)
- Random pattern
- 10,000 allocations/deallocations

### Performance Benchmarking
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

## Integration

### API Requirements

Your allocator must implement these functions:

```c
void* my_malloc(size_t size);
void my_free(void* ptr);
void* my_realloc(void* ptr, size_t size);  // optional
void* my_calloc(size_t nmemb, size_t size);  // optional
```

### Example: Testing OpenAlloc

```c
#include "test_allocator.h"
#include "openalloc.h"

int main(void) {
    // Initialize your allocator
    #define HEAP_SIZE (100 * 1024 * 1024)
    static unsigned char heap[HEAP_SIZE];
    openalloc_init(heap, HEAP_SIZE);
    
    // Create interface
    allocator_interface_t iface = {
        .malloc = openalloc_malloc,
        .free = openalloc_free,
        .realloc = openalloc_realloc,
        .calloc = NULL  // Not implemented
    };
    
    // Run tests
    test_allocator_set_interface(&iface);
    test_allocator_run_all(12345);
    test_allocator_run_benchmark(12345);
    
    return 0;
}
```

### Example: Testing glibc malloc

```c
#include "test_allocator.h"
#include <stdlib.h>

int main(void) {
    allocator_interface_t iface = {
        .malloc = malloc,
        .free = free,
        .realloc = realloc,
        .calloc = calloc
    };
    
    test_allocator_set_interface(&iface);
    test_allocator_run_all(12345);
    test_allocator_run_benchmark(12345);
    
    return 0;
}
```

## Output Format

```
=== Running Test Suite (seed=12345) ===
  [RUN] basic_correctness
  [PASS] basic_correctness
  [RUN] adjacent_allocations
  [PASS] adjacent_allocations
  [RUN] alignment
  [PASS] alignment
  [RUN] realloc_basic
  [PASS] realloc_basic
  [RUN] calloc_basic
  [SKIP] calloc not implemented
  [RUN] double_free_detection
  [PASS] double_free_detection (no crash)
  ...

=== Test Summary ===
  Passed:  13
  Failed:  0
  Total:   13

✓ All tests passed!

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

The test harness captures signals and provides detailed failure information:
- Test name
- Assertion location (file:line)
- Condition that failed

## Performance Targets

- Runtime: <1 second for full test suite
- Peak RSS: <100 MB
- Benchmark: <30 seconds for 1M operations

## Files

- `test_allocator.h` - Test harness API
- `test_allocator.c` - Main test implementation
- `Makefile.test` - Build configuration
- `example_usage.c` - Integration examples

## Make Targets

```bash
make -f Makefile.test all          # Build all executables
make -f Makefile.test run-test     # Run original test suite
make -f Makefile.test run-test-allocator  # Run comprehensive test suite
make -f Makefile.test benchmark    # Run performance benchmark
make -f Makefile.test test-full    # Run all tests
make -f Makefile.test clean        # Remove build artifacts
make -f Makefile.test help         # Show help message
```

## Troubleshooting

**Tests hang**: Check allocator locking in multithreading tests
**Segmentation faults**: Verify allocator handles edge cases (NULL, zero-size)
**High fragmentation**: Review coalescing implementation
**Poor performance**: Check lock contention and block splitting logic
**OOM in benchmark**: Reduce BENCHMARK_ITERATIONS or increase heap size

## License

This test harness is provided as-is for testing custom memory allocators.
