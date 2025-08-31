# Example 06: Memory Management with Heap_4

## Overview

This example demonstrates FreeRTOS heap_4 memory management, showcasing dynamic allocation patterns, fragmentation prevention through coalescence, memory monitoring, and long-term stability. It simulates various real-world memory usage patterns found in embedded systems.

## Key Features Demonstrated

### 1. Dynamic Memory Allocation
- Variable-size allocations (32-1024 bytes)
- Frequent allocation/deallocation cycles
- Graceful handling of allocation failures

### 2. Memory Coalescence
- Adjacent free blocks automatically merge
- Prevents long-term fragmentation
- Demonstrated through fragmentation test

### 3. Memory Pool Pattern
- Fixed-size block allocation
- Zero fragmentation for uniform sizes
- Faster allocation than heap

### 4. Variable-Length Messages
- Flexible array members for data
- Queue-based message passing
- Proper cleanup on queue overflow

### 5. Dynamic String Management
- Growing string buffers
- Automatic capacity expansion
- Safe string operations

### 6. Heap Monitoring
- Real-time statistics tracking
- Fragmentation detection
- Low memory warnings

## System Architecture

```
Memory Layout:
┌─────────────────────────────────────┐
│         Total Heap (256KB)          │
├─────────────────────────────────────┤
│                                     │
│  ┌──────────┐  ┌──────────┐       │
│  │ Allocated │  │   Free   │  ...  │
│  └──────────┘  └──────────┘       │
│                                     │
│  Memory Pool (Fixed Region)         │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┐│
│  │B0 │B1 │B2 │B3 │B4 │B5 │B6 │... ││
│  └───┴───┴───┴───┴───┴───┴───┴───┘│
│                                     │
└─────────────────────────────────────┘

Task Memory Operations:
AllocationTask ──> Random size alloc/free
ProducerTask ───> Create variable messages
ConsumerTask ───> Process and free messages
StringTask ─────> Dynamic string operations
PoolTask ───────> Fixed-size pool usage
FragmentTask ───> Test coalescence
MonitorTask ────> Track heap statistics
StressTask ─────> Intensive testing
```

## Tasks Description

### 1. Allocation Task (Priority 3)
Performs continuous random-size allocations:
- Sizes: 32-1024 bytes randomly
- Maintains 10 active allocations
- Cycles through allocation slots
- Writes test pattern to verify integrity

### 2. Message Producer/Consumer (Priority 4)
Simulates variable-length message system:
- **Producer**: Creates messages with random data length
- **Consumer**: Processes messages and verifies data
- Uses queue to pass message pointers
- Proper cleanup on queue overflow

### 3. String Task (Priority 2)
Dynamic string buffer operations:
- Creates expandable string buffers
- Appends data incrementally
- Automatically grows capacity
- Demonstrates safe string handling

### 4. Pool Task (Priority 2)
Fixed-size memory pool usage:
- Allocates from pre-allocated pool
- No fragmentation for fixed sizes
- Faster than heap allocation
- Tracks usage statistics

### 5. Fragmentation Task (Priority 1)
Tests heap_4 coalescence feature:
- Allocates many blocks
- Frees every other block (fragments)
- Frees remaining (tests merge)
- Verifies coalescence success

### 6. Monitor Task (Priority 5)
Heap health monitoring:
- Reports statistics every 5 seconds
- Warns on low memory (<10% free)
- Detects high fragmentation
- Tracks peak usage

### 7. Stress Task (Priority 1)
Optional intensive testing:
- 1000 rapid allocation cycles
- Random sizes and hold times
- Pattern verification
- Memory corruption detection

## Expected Output

```
===========================================
Example 06: Memory Management with Heap_4
Dynamic allocation and fragmentation prevention
Total heap size: 262144 bytes
===========================================

[POOL] Initializing memory pool (10 blocks x 256 bytes)
[POOL] Memory pool initialized successfully

========================================
HEAP STATISTICS
========================================
Total Heap:      262144 bytes
Current Free:    259584 bytes (99.0%)
Minimum Ever:    259584 bytes
Peak Usage:      2560 bytes
Allocations:     0
Deallocations:   0
Failed Allocs:   0
Fragmentation:   0.0%
========================================

[ALLOC] Task started - variable size allocations
[PRODUCER] Task started - variable length messages
[CONSUMER] Task started - processing messages
[STRING] Task started - dynamic string operations
[POOL] Task started - fixed size allocations
[FRAGMENT] Task started - testing coalescence
[MONITOR] Heap monitor started

[ALLOC] Completed 100 allocations
[CONSUMER] Message 5: 147 bytes at tick 2543
[STRING] Built: Sensor Data: [0:3.40] [1:7.80] ... (len=95, cap=128)
[POOL] Usage: 3/10 blocks, Total allocations: 24

[FRAGMENT] Allocating 50 blocks...
[FRAGMENT] Creating fragmentation...
[FRAGMENT] Free heap after fragmentation: 135680
[FRAGMENT] Testing coalescence...
[FRAGMENT] Free heap after coalescence: 256896
[FRAGMENT] Coalescence successful!

========================================
HEAP STATISTICS
========================================
Total Heap:      262144 bytes
Current Free:    245760 bytes (93.7%)
Minimum Ever:    125952 bytes
Peak Usage:      136192 bytes
Allocations:     487
Deallocations:   412
Failed Allocs:   0
Fragmentation:   8.7%
========================================
```

## Memory Patterns Demonstrated

### 1. Allocation Pattern Analysis
```c
// Random size allocation simulates real-world usage
size = rand() % (1024 - 32) + 32;  // 32 to 1024 bytes

// Result: Tests heap_4's ability to handle various sizes
// Without coalescence: Would fragment quickly
// With coalescence: Maintains low fragmentation
```

### 2. Memory Pool Benefits
```c
// Fixed-size allocations from pool
// - No fragmentation
// - O(1) allocation time
// - Predictable memory usage
// - Ideal for fixed-size structures
```

### 3. Variable-Length Messages
```c
// Flexible array member pattern
typedef struct {
    uint16_t length;
    uint8_t data[];  // Size determined at runtime
} Message_t;

// Allocate exact size needed
Message_t* msg = pvPortMalloc(sizeof(Message_t) + data_len);
```

## Performance Metrics

### Heap_4 Characteristics
| Operation | Typical Time | Complexity |
|-----------|-------------|------------|
| Allocate (small) | 200-500 cycles | O(n) free blocks |
| Allocate (large) | 500-1500 cycles | O(n) free blocks |
| Free | 300-800 cycles | O(1) + coalesce |
| Coalescence | 100-300 cycles | O(1) adjacent |

### Memory Overhead
- Each allocation: 8 bytes overhead
- Pool block: 0 bytes overhead
- String buffer: sizeof(StringBuffer_t) + data

## Exercises

1. **Enable Stress Test**
   - Set `g_stress_test_running = true`
   - Observe behavior under load
   - Monitor fragmentation levels

2. **Adjust Heap Size**
   - Reduce `configTOTAL_HEAP_SIZE`
   - Observe allocation failures
   - Test recovery mechanisms

3. **Modify Pool Size**
   - Change `POOL_BLOCK_SIZE`
   - Adjust `POOL_BLOCK_COUNT`
   - Compare with heap allocation

4. **Add Memory Leak**
   - Comment out a `vPortFree()`
   - Watch heap depletion
   - Use monitor to detect

5. **Implement Best-Fit**
   - Modify allocation strategy
   - Compare fragmentation
   - Measure performance

## Wind Turbine Application

This example models memory usage in the wind turbine system:

1. **Sensor Data Buffers**
   - Variable-size based on sample count
   - Dynamic allocation for efficiency
   - Proper cleanup on transmission

2. **Message Queuing**
   - Variable-length status messages
   - Alert notifications
   - Network packets

3. **Configuration Storage**
   - Dynamic configuration structures
   - Runtime parameter updates
   - String-based settings

4. **Data Logging**
   - Circular buffers
   - Event logs
   - Diagnostic traces

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/06_heap_demo/heap_demo_example

# Observe:
# - Heap statistics every 5 seconds
# - Fragmentation test results
# - Memory pool efficiency
# - String buffer growth
```

## Key Takeaways

1. **Heap_4 prevents fragmentation** through coalescence
2. **Memory pools** eliminate fragmentation for fixed sizes
3. **Monitor heap health** continuously in production
4. **Handle allocation failures** gracefully
5. **Pre-allocate when possible** to avoid runtime failures
6. **Use flexible arrays** for variable-length data
7. **Free in reverse order** of allocation when possible
8. **Track statistics** to detect leaks and fragmentation

## Troubleshooting

### High Fragmentation
- Increase heap size temporarily
- Use memory pools for common sizes
- Reduce allocation frequency
- Consider heap_5 for multiple regions

### Allocation Failures
- Check heap statistics
- Look for memory leaks
- Reduce peak usage
- Implement fallback strategies

### Memory Corruption
- Enable stack overflow detection
- Check array bounds
- Verify pointer validity
- Use memory patterns for debugging

## Integration with Final System

This memory management capability is crucial for the integrated Wind Turbine Monitor:

### Direct Applications

1. **Dynamic Buffer Management**
   - Variable-size sensor data buffers
   - Network packet allocation
   - Adaptive to data rates

2. **Message System**
   - Variable-length alert messages
   - Efficient queue memory usage
   - No fragmentation with proper management

3. **Configuration Storage**
   - Runtime threshold updates
   - Dynamic parameter structures
   - String-based settings

### Console Dashboard Preview

```
MEMORY MONITOR                        Free: 245KB/256KB
----------------------------------------------------------
HEAP STATUS:
  Usage: [####----------------] 4% (11KB/256KB)
  Free: 245KB            Minimum Ever: 125KB
  Fragmentation: 2.1%    Largest Block: 240KB
  
ALLOCATIONS:
  Active: 42             Failed: 0
  Peak: 136KB           Average Size: 256B
  
POOL STATUS:
  Sensor Pool: 8/16 blocks used
  Message Pool: 3/32 blocks used
  Network Pool: 2/8 blocks used
```

## Next Steps

After mastering memory management, proceed to:
- Example 07: Stack Overflow Detection
- Then continue through remaining capabilities
- Finally integrate all 8 capabilities into complete system