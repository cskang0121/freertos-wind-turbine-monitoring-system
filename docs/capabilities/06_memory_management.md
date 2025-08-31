# Capability 6: Memory Management with Heap_4

## Overview

Memory management is critical in embedded systems where RAM is limited and applications must run reliably for extended periods. FreeRTOS provides five heap management schemes, with heap_4 offering the best balance of features for most production applications through its memory coalescence capability that prevents fragmentation.

## FreeRTOS Memory Schemes Comparison

### Heap_1: Simplest (No Free)
```c
// Characteristics:
// - Allocate only, no free
// - Deterministic
// - No fragmentation (can't free!)

// Use case: 
// - Systems that never delete tasks/queues
// - All allocation at startup
```

### Heap_2: Simple (No Coalescence)
```c
// Characteristics:
// - Allows free
// - No adjacent block merging
// - Fragmentation over time

// Use case:
// - Systems with same-size allocations
// - Short-running applications
```

### Heap_3: Wrapper (Uses malloc/free)
```c
// Characteristics:
// - Thread-safe wrapper around standard malloc
// - Requires linker configuration
// - Non-deterministic timing

// Use case:
// - Systems with existing C library
// - Non-real-time requirements
```

### Heap_4: Production Ready (Coalescence)
```c
// Characteristics:
// - First-fit algorithm
// - Adjacent free blocks merge (coalescence)
// - Prevents long-term fragmentation
// - Deterministic (bounded time)

// Use case:
// - Most production systems
// - Long-running applications
// - Variable-size allocations
```

### Heap_5: Multiple Regions
```c
// Characteristics:
// - Spans non-contiguous memory
// - Useful for external RAM
// - Based on heap_4 algorithm

// Use case:
// - Systems with multiple RAM regions
// - External SDRAM usage
```

## Why Heap_4 for Production?

### Coalescence Prevents Fragmentation

```
Without Coalescence (heap_2):
Initial: [████████████████████████] 100% free

After allocations:
[AA][BB][CC][DD][████████] 

After freeing B and C:
[AA][  ][  ][DD][████████]
     ↑    ↑
   Free  Free (but separate!)
   
Cannot allocate size > individual free blocks!

With Coalescence (heap_4):
After freeing B and C:
[AA][      ][DD][████████]
     ↑
   Merged into single block!
   
Can now allocate larger sizes!
```

## Memory Management APIs

### Basic Allocation

```c
// Allocate memory (thread-safe)
void *pvPortMalloc(size_t xSize);

// Free memory (thread-safe)
void vPortFree(void *pv);

// Get free heap size
size_t xPortGetFreeHeapSize(void);

// Get minimum ever free heap size (high water mark)
size_t xPortGetMinimumEverFreeHeapSize(void);

// Example usage:
uint8_t *buffer = pvPortMalloc(1024);
if (buffer != NULL) {
    // Use buffer
    memset(buffer, 0, 1024);
    
    // Free when done
    vPortFree(buffer);
    buffer = NULL;  // Prevent use-after-free
}
```

### Heap Configuration

```c
// In FreeRTOSConfig.h
#define configTOTAL_HEAP_SIZE ((size_t)(256 * 1024))  // 256KB heap

// Optional heap location (normally in .bss)
#if configAPPLICATION_ALLOCATED_HEAP == 1
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".ccm")));
#endif
```

### Failed Allocation Hook

```c
// Called when allocation fails
void vApplicationMallocFailedHook(void) {
    // Log the failure
    size_t freeHeap = xPortGetFreeHeapSize();
    printf("Malloc failed! Free heap: %zu\n", freeHeap);
    
    // System-specific recovery
    // - Free cached data
    // - Trigger garbage collection
    // - Enter safe mode
    
    // Or halt system
    taskDISABLE_INTERRUPTS();
    for(;;);
}
```

## Implementation Patterns

### 1. Memory Pool Pattern

```c
// Pre-allocate fixed-size blocks to avoid fragmentation
typedef struct {
    uint8_t data[256];
    bool in_use;
} MemoryBlock_t;

#define POOL_SIZE 10
static MemoryBlock_t* memory_pool = NULL;

void pool_init(void) {
    // Single large allocation
    memory_pool = pvPortMalloc(sizeof(MemoryBlock_t) * POOL_SIZE);
    if (memory_pool != NULL) {
        for (int i = 0; i < POOL_SIZE; i++) {
            memory_pool[i].in_use = false;
        }
    }
}

MemoryBlock_t* pool_alloc(void) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (!memory_pool[i].in_use) {
            memory_pool[i].in_use = true;
            return &memory_pool[i];
        }
    }
    return NULL;  // Pool exhausted
}

void pool_free(MemoryBlock_t* block) {
    block->in_use = false;
}
```

### 2. Dynamic String Handling

```c
// Safe string duplication
char* safe_strdup(const char* src) {
    if (src == NULL) return NULL;
    
    size_t len = strlen(src) + 1;
    char* dst = pvPortMalloc(len);
    
    if (dst != NULL) {
        strcpy(dst, src);
    }
    
    return dst;
}

// String buffer management
typedef struct {
    char* buffer;
    size_t size;
    size_t used;
} StringBuffer_t;

StringBuffer_t* string_buffer_create(size_t initial_size) {
    StringBuffer_t* sb = pvPortMalloc(sizeof(StringBuffer_t));
    if (sb == NULL) return NULL;
    
    sb->buffer = pvPortMalloc(initial_size);
    if (sb->buffer == NULL) {
        vPortFree(sb);
        return NULL;
    }
    
    sb->size = initial_size;
    sb->used = 0;
    return sb;
}

void string_buffer_destroy(StringBuffer_t* sb) {
    if (sb != NULL) {
        vPortFree(sb->buffer);
        vPortFree(sb);
    }
}
```

### 3. Variable-Length Message Queue

```c
// Queue for variable-length messages
typedef struct {
    size_t length;
    uint8_t data[];  // Flexible array member
} Message_t;

QueueHandle_t xMessageQueue;

void send_variable_message(const uint8_t* data, size_t len) {
    // Allocate message with exact size needed
    Message_t* msg = pvPortMalloc(sizeof(Message_t) + len);
    
    if (msg != NULL) {
        msg->length = len;
        memcpy(msg->data, data, len);
        
        // Send pointer through queue
        if (xQueueSend(xMessageQueue, &msg, timeout) != pdTRUE) {
            // Failed to queue - free memory
            vPortFree(msg);
        }
    }
}

void receive_variable_message(void) {
    Message_t* msg;
    
    if (xQueueReceive(xMessageQueue, &msg, timeout) == pdTRUE) {
        // Process message
        process_data(msg->data, msg->length);
        
        // Free after processing
        vPortFree(msg);
    }
}
```

### 4. Heap Monitoring

```c
typedef struct {
    size_t current_free;
    size_t minimum_free;
    size_t total_size;
    size_t largest_free_block;
    uint32_t allocation_count;
    uint32_t free_count;
    float fragmentation_ratio;
} HeapStats_t;

void heap_get_stats(HeapStats_t* stats) {
    stats->current_free = xPortGetFreeHeapSize();
    stats->minimum_free = xPortGetMinimumEverFreeHeapSize();
    stats->total_size = configTOTAL_HEAP_SIZE;
    stats->largest_free_block = xPortGetLargestFreeBlock();  // Custom function
    
    // Calculate fragmentation
    if (stats->current_free > 0) {
        stats->fragmentation_ratio = 1.0f - 
            ((float)stats->largest_free_block / stats->current_free);
    }
}

void heap_monitor_task(void* pvParameters) {
    HeapStats_t stats;
    
    for (;;) {
        heap_get_stats(&stats);
        
        printf("Heap: Free=%zu/%zu, Min=%zu, Frag=%.1f%%\n",
               stats.current_free,
               stats.total_size,
               stats.minimum_free,
               stats.fragmentation_ratio * 100);
        
        // Alert on low memory
        if (stats.current_free < (stats.total_size * 0.1)) {
            printf("WARNING: Heap usage > 90%%!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## Best Practices

### 1. Minimize Dynamic Allocation

```c
// BAD - Frequent allocation/deallocation
void process_data(void) {
    for (int i = 0; i < 1000; i++) {
        uint8_t* temp = pvPortMalloc(256);
        // Use temp
        vPortFree(temp);
    }
}

// GOOD - Reuse buffer
void process_data(void) {
    uint8_t* buffer = pvPortMalloc(256);
    
    for (int i = 0; i < 1000; i++) {
        // Reuse same buffer
        memset(buffer, 0, 256);
        // Process...
    }
    
    vPortFree(buffer);
}
```

### 2. Check All Allocations

```c
// ALWAYS check allocation success
SensorData_t* data = pvPortMalloc(sizeof(SensorData_t));
if (data == NULL) {
    // Handle allocation failure
    log_error("Failed to allocate sensor data");
    return ERROR_NO_MEMORY;
}
```

### 3. Prevent Memory Leaks

```c
// Use RAII-style patterns where possible
typedef struct {
    uint8_t* buffer;
    size_t size;
} Resource_t;

Resource_t* resource_create(size_t size) {
    Resource_t* res = pvPortMalloc(sizeof(Resource_t));
    if (res == NULL) return NULL;
    
    res->buffer = pvPortMalloc(size);
    if (res->buffer == NULL) {
        vPortFree(res);  // Clean up partial allocation
        return NULL;
    }
    
    res->size = size;
    return res;
}

void resource_destroy(Resource_t* res) {
    if (res != NULL) {
        vPortFree(res->buffer);  // Free in reverse order
        vPortFree(res);
    }
}
```

### 4. Pre-allocate at Startup

```c
// Allocate during initialization
static uint8_t* g_sensor_buffer = NULL;
static uint8_t* g_network_buffer = NULL;

bool system_init(void) {
    // All allocation at startup
    g_sensor_buffer = pvPortMalloc(SENSOR_BUFFER_SIZE);
    g_network_buffer = pvPortMalloc(NETWORK_BUFFER_SIZE);
    
    if (g_sensor_buffer == NULL || g_network_buffer == NULL) {
        // Cleanup and fail
        system_cleanup();
        return false;
    }
    
    return true;
}
```

## Common Pitfalls

### 1. Use After Free

```c
// BUG - Using freed memory
uint8_t* buffer = pvPortMalloc(100);
vPortFree(buffer);
buffer[0] = 0x42;  // CRASH! Use after free

// FIX - NULL after free
uint8_t* buffer = pvPortMalloc(100);
vPortFree(buffer);
buffer = NULL;  // Prevent accidental use
```

### 2. Double Free

```c
// BUG - Freeing twice
vPortFree(buffer);
// ... later ...
vPortFree(buffer);  // CRASH! Double free

// FIX - Defensive programming
void safe_free(void** ptr) {
    if (ptr != NULL && *ptr != NULL) {
        vPortFree(*ptr);
        *ptr = NULL;
    }
}
```

### 3. Memory Leak in Error Path

```c
// BUG - Leak on error
void* func(void) {
    uint8_t* buf1 = pvPortMalloc(100);
    uint8_t* buf2 = pvPortMalloc(200);
    
    if (buf2 == NULL) {
        return NULL;  // LEAK! buf1 not freed
    }
    
    // ...
}

// FIX - Clean up on all paths
void* func(void) {
    uint8_t* buf1 = pvPortMalloc(100);
    if (buf1 == NULL) return NULL;
    
    uint8_t* buf2 = pvPortMalloc(200);
    if (buf2 == NULL) {
        vPortFree(buf1);  // Clean up
        return NULL;
    }
    
    // ...
}
```

## Wind Turbine Application

### Dynamic Buffer Management

```c
// Sensor data with variable sample counts
typedef struct {
    uint32_t timestamp;
    uint16_t sample_count;
    float samples[];  // Flexible array
} SensorPacket_t;

SensorPacket_t* create_sensor_packet(uint16_t num_samples) {
    size_t size = sizeof(SensorPacket_t) + (num_samples * sizeof(float));
    SensorPacket_t* packet = pvPortMalloc(size);
    
    if (packet != NULL) {
        packet->timestamp = xTaskGetTickCount();
        packet->sample_count = num_samples;
    }
    
    return packet;
}

// Adaptive logging based on available memory
void adaptive_logger(const char* message) {
    size_t free_heap = xPortGetFreeHeapSize();
    
    if (free_heap > (configTOTAL_HEAP_SIZE * 0.5)) {
        // Plenty of memory - full logging
        log_verbose(message);
    } else if (free_heap > (configTOTAL_HEAP_SIZE * 0.2)) {
        // Low memory - essential only
        log_essential(message);
    } else {
        // Critical - no allocation
        // Use static emergency buffer
    }
}
```

## Performance Considerations

### Heap_4 Algorithm Performance

```c
// First-fit algorithm
// Time complexity: O(n) where n = number of free blocks
// Typical times on Cortex-M:
//   Small allocation (< 256 bytes): ~200-500 cycles
//   Large allocation (> 1KB): ~500-1500 cycles
//   Free operation: ~300-800 cycles (includes coalescing)

// Fragmentation resistance:
// - Coalescence merges adjacent free blocks
// - First-fit tends to use lower addresses first
// - Result: Fragmentation stays low in typical use
```

### Memory Overhead

```c
// Each allocated block has overhead:
// - Block size: 4 bytes
// - Next pointer: 4 bytes  
// - Total: 8 bytes per allocation

// Example:
// Request 10 bytes → Actually uses 18 bytes
// Request 100 bytes → Actually uses 108 bytes
// Overhead percentage: 8/size * 100%
```

## Testing Memory Management

### 1. Fragmentation Test

```c
void test_fragmentation(void) {
    #define NUM_BLOCKS 100
    void* blocks[NUM_BLOCKS];
    
    // Allocate all blocks
    for (int i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = pvPortMalloc(rand() % 256 + 32);
    }
    
    // Free every other block (creates fragmentation)
    for (int i = 0; i < NUM_BLOCKS; i += 2) {
        vPortFree(blocks[i]);
    }
    
    // Free remaining (coalescence should merge)
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        vPortFree(blocks[i]);
    }
    
    // Check fragmentation level
    size_t free_heap = xPortGetFreeHeapSize();
    size_t largest_block = xPortGetLargestFreeBlock();
    
    float fragmentation = 1.0f - ((float)largest_block / free_heap);
    configASSERT(fragmentation < 0.1f);  // Should be low
}
```

### 2. Stress Test

```c
void stress_test_task(void* pvParameters) {
    uint32_t cycles = 0;
    
    while (cycles < 10000) {
        // Random size allocation
        size_t size = (rand() % 1024) + 32;
        void* ptr = pvPortMalloc(size);
        
        if (ptr != NULL) {
            // Use memory
            memset(ptr, 0xAA, size);
            
            // Random delay
            vTaskDelay(rand() % 10);
            
            // Free
            vPortFree(ptr);
        }
        
        cycles++;
    }
}
```

### 3. Leak Detection

```c
void leak_detection_test(void) {
    size_t initial_free = xPortGetFreeHeapSize();
    
    // Run allocation/deallocation cycles
    for (int i = 0; i < 1000; i++) {
        void* ptr = pvPortMalloc(100);
        // Deliberately missing vPortFree to detect
    }
    
    size_t final_free = xPortGetFreeHeapSize();
    size_t leaked = initial_free - final_free;
    
    printf("Memory leaked: %zu bytes\n", leaked);
}
```

## Summary

Heap_4 provides robust memory management for embedded systems:

1. **Coalescence prevents fragmentation** - Adjacent free blocks merge
2. **Deterministic timing** - Bounded allocation time
3. **Thread-safe operations** - Built-in protection
4. **Monitoring capabilities** - Track usage and fragmentation
5. **Failure handling** - Graceful out-of-memory response
6. **Long-term stability** - Suitable for 24/7 operation

Key principles:
- Minimize dynamic allocation
- Check all allocations
- Free in reverse order
- Pre-allocate when possible
- Monitor heap health
- Handle failures gracefully