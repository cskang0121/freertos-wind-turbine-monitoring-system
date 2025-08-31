# Memory Management with Heap_4 - Quick Reference

## Overview
This capability demonstrates dynamic memory management using FreeRTOS heap_4 scheme. Heap_4 provides deterministic allocation/deallocation with coalescence to prevent fragmentation.

**Key Concepts:**
- Dynamic memory allocation with pvPortMalloc/vPortFree
- Memory fragmentation prevention through coalescence
- Heap_4 scheme for predictable behavior
- Memory leak detection and prevention
- Runtime memory monitoring

## FreeRTOS Memory Management Schemes

| Scheme | Features | Use Case |
|--------|----------|----------|
| **heap_1** | Allocate only, no free | Simple systems, no deallocation |
| **heap_2** | Best fit, no coalescence | Fixed-size blocks, fragmentation OK |
| **heap_3** | Wraps malloc/free | Thread-safe wrapper for libc |
| **heap_4** | **Coalescence, deterministic** | **Most embedded systems** |
| **heap_5** | Multiple memory regions | Complex memory layouts |

## Heap_4 Characteristics

### Memory Layout
```
Static Memory: [.text] [.data] [.bss] [stack]
                                      ↓
Dynamic Memory: [Heap Space - configTOTAL_HEAP_SIZE]
                [Free Block] [Used Block] [Free Block]
```

### Key Features
- **Coalescence**: Adjacent free blocks automatically merge
- **Deterministic**: Predictable allocation/deallocation time
- **First fit**: Fast allocation algorithm
- **Memory protection**: Overrun detection with guard bytes
- **Thread-safe**: Built-in critical section protection

## Core Memory Operations

### 1. Basic Allocation/Deallocation
```c
// Allocate memory
void* pvPortMalloc(size_t xSize);

// Free memory
void vPortFree(void* pv);

// Get heap statistics
void vPortGetHeapStats(HeapStats_t* pxHeapStats);

// Get minimum ever free heap size
size_t xPortGetMinimumEverFreeHeapSize(void);
```

### 2. Heap Statistics Structure
```c
typedef struct xHeapStats
{
    size_t xAvailableHeapSpaceInBytes;      // Current free bytes
    size_t xSizeOfLargestFreeBlockInBytes;  // Largest single block
    size_t xSizeOfSmallestFreeBlockInBytes; // Smallest single block
    size_t xNumberOfFreeBlocks;             // Number of free blocks
    size_t xMinimumEverFreeBytesRemaining;  // Minimum free ever seen
    size_t xNumberOfSuccessfulAllocations;  // Total successful allocs
    size_t xNumberOfSuccessfulFrees;        // Total successful frees
} HeapStats_t;
```

## Implementation Patterns

### 1. Safe Memory Allocation
```c
typedef struct {
    float* sensor_data;
    uint32_t sample_count;
    TickType_t timestamp;
} SensorBuffer_t;

SensorBuffer_t* create_sensor_buffer(uint32_t samples) {
    // Allocate buffer structure
    SensorBuffer_t* buffer = (SensorBuffer_t*)pvPortMalloc(sizeof(SensorBuffer_t));
    if(buffer == NULL) {
        return NULL; // Allocation failed
    }
    
    // Allocate data array
    buffer->sensor_data = (float*)pvPortMalloc(samples * sizeof(float));
    if(buffer->sensor_data == NULL) {
        // Cleanup partial allocation
        vPortFree(buffer);
        return NULL;
    }
    
    // Initialize structure
    buffer->sample_count = samples;
    buffer->timestamp = xTaskGetTickCount();
    
    return buffer;
}

void destroy_sensor_buffer(SensorBuffer_t* buffer) {
    if(buffer != NULL) {
        if(buffer->sensor_data != NULL) {
            vPortFree(buffer->sensor_data);
        }
        vPortFree(buffer);
    }
}
```

### 2. Memory Pool Pattern
```c
// Fixed-size memory pool for common allocations
#define POOL_BLOCK_SIZE     128
#define POOL_BLOCK_COUNT    10

typedef struct {
    uint8_t data[POOL_BLOCK_SIZE];
    bool in_use;
} PoolBlock_t;

typedef struct {
    PoolBlock_t blocks[POOL_BLOCK_COUNT];
    SemaphoreHandle_t pool_mutex;
    uint32_t blocks_used;
} MemoryPool_t;

MemoryPool_t g_memory_pool;

void* pool_alloc(void) {
    void* result = NULL;
    
    if(xSemaphoreTake(g_memory_pool.pool_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Find free block
        for(int i = 0; i < POOL_BLOCK_COUNT; i++) {
            if(!g_memory_pool.blocks[i].in_use) {
                g_memory_pool.blocks[i].in_use = true;
                g_memory_pool.blocks_used++;
                result = g_memory_pool.blocks[i].data;
                break;
            }
        }
        xSemaphoreGive(g_memory_pool.pool_mutex);
    }
    
    return result;
}
```

### 3. Dynamic String Management
```c
typedef struct {
    char* buffer;
    size_t capacity;
    size_t length;
} DynamicString_t;

DynamicString_t* string_create(size_t initial_capacity) {
    DynamicString_t* str = (DynamicString_t*)pvPortMalloc(sizeof(DynamicString_t));
    if(str == NULL) return NULL;
    
    str->buffer = (char*)pvPortMalloc(initial_capacity + 1); // +1 for null terminator
    if(str->buffer == NULL) {
        vPortFree(str);
        return NULL;
    }
    
    str->capacity = initial_capacity;
    str->length = 0;
    str->buffer[0] = '\0';
    
    return str;
}

bool string_append(DynamicString_t* str, const char* text) {
    size_t text_len = strlen(text);
    size_t required_capacity = str->length + text_len;
    
    // Resize if necessary
    if(required_capacity >= str->capacity) {
        size_t new_capacity = (required_capacity + 1) * 2; // Double with headroom
        char* new_buffer = (char*)pvPortMalloc(new_capacity);
        if(new_buffer == NULL) {
            return false; // Allocation failed
        }
        
        // Copy existing content
        memcpy(new_buffer, str->buffer, str->length + 1);
        vPortFree(str->buffer);
        
        str->buffer = new_buffer;
        str->capacity = new_capacity - 1;
    }
    
    // Append new text
    strcpy(str->buffer + str->length, text);
    str->length += text_len;
    
    return true;
}
```

## Memory Monitoring and Debugging

### 1. Heap Health Monitoring
```c
void monitor_heap_health(void) {
    HeapStats_t heap_stats;
    vPortGetHeapStats(&heap_stats);
    
    // Calculate fragmentation
    float fragmentation = 0.0f;
    if(heap_stats.xAvailableHeapSpaceInBytes > 0) {
        fragmentation = 100.0f * (1.0f - 
            (float)heap_stats.xSizeOfLargestFreeBlockInBytes / 
            (float)heap_stats.xAvailableHeapSpaceInBytes);
    }
    
    // Log critical conditions
    if(fragmentation > 50.0f) {
        printf("WARNING: Heap fragmentation %.1f%%\n", fragmentation);
    }
    
    if(heap_stats.xAvailableHeapSpaceInBytes < 1024) {
        printf("CRITICAL: Low memory - %d bytes remaining\n", 
               (int)heap_stats.xAvailableHeapSpaceInBytes);
    }
}
```

### 2. Allocation Failure Hook
```c
// Called when pvPortMalloc fails
void vApplicationMallocFailedHook(void) {
    // Log failure details
    HeapStats_t stats;
    vPortGetHeapStats(&stats);
    
    printf("MALLOC FAILED: %d bytes available, largest block %d\n",
           (int)stats.xAvailableHeapSpaceInBytes,
           (int)stats.xSizeOfLargestFreeBlockInBytes);
    
    // Attempt emergency cleanup
    emergency_memory_cleanup();
    
    // In production, might trigger system reset
    // portDISABLE_INTERRUPTS();
    // for(;;);
}
```

### 3. Memory Leak Detection
```c
typedef struct {
    void* address;
    size_t size;
    const char* file;
    int line;
    TickType_t timestamp;
} AllocRecord_t;

#define MAX_ALLOC_RECORDS 50
AllocRecord_t g_alloc_records[MAX_ALLOC_RECORDS];
int g_alloc_count = 0;

// Wrapper for debugging allocations
void* debug_malloc(size_t size, const char* file, int line) {
    void* ptr = pvPortMalloc(size);
    
    if(ptr && g_alloc_count < MAX_ALLOC_RECORDS) {
        g_alloc_records[g_alloc_count].address = ptr;
        g_alloc_records[g_alloc_count].size = size;
        g_alloc_records[g_alloc_count].file = file;
        g_alloc_records[g_alloc_count].line = line;
        g_alloc_records[g_alloc_count].timestamp = xTaskGetTickCount();
        g_alloc_count++;
    }
    
    return ptr;
}

#define MALLOC(size) debug_malloc(size, __FILE__, __LINE__)
```

## Fragmentation Prevention Strategies

### 1. Size Class Pools
```c
// Separate pools for different size classes
#define SMALL_POOL_SIZE     32      // For small objects
#define MEDIUM_POOL_SIZE    128     // For medium objects  
#define LARGE_POOL_SIZE     512     // For large objects

void* size_aware_alloc(size_t size) {
    if(size <= SMALL_POOL_SIZE) {
        return small_pool_alloc();
    } else if(size <= MEDIUM_POOL_SIZE) {
        return medium_pool_alloc();
    } else if(size <= LARGE_POOL_SIZE) {
        return large_pool_alloc();
    } else {
        // Use heap for very large allocations
        return pvPortMalloc(size);
    }
}
```

### 2. Allocation Patterns
```c
// Good: Allocate all needed memory upfront
void good_allocation_pattern(void) {
    // Allocate everything at once
    buffer1 = pvPortMalloc(1024);
    buffer2 = pvPortMalloc(2048);
    buffer3 = pvPortMalloc(512);
    
    // Use buffers...
    
    // Free in reverse order (optional but good practice)
    vPortFree(buffer3);
    vPortFree(buffer2);
    vPortFree(buffer1);
}

// Bad: Frequent alloc/free causes fragmentation  
void bad_allocation_pattern(void) {
    for(int i = 0; i < 100; i++) {
        void* temp = pvPortMalloc(random_size());
        // ... some work ...
        vPortFree(temp);  // Creates fragmentation
    }
}
```

## Integration with Wind Turbine System

### Dynamic Message Queues
Our system uses dynamic allocation for variable-size messages:

```c
typedef struct {
    MessageType_t type;
    size_t data_size;
    uint8_t data[];  // Variable-length data
} VariableMessage_t;

// Send variable-size message
bool send_variable_message(QueueHandle_t queue, MessageType_t type, 
                          const void* data, size_t size) {
    // Allocate message with exact size needed
    size_t total_size = sizeof(VariableMessage_t) + size;
    VariableMessage_t* msg = (VariableMessage_t*)pvPortMalloc(total_size);
    
    if(msg == NULL) {
        return false; // Allocation failed
    }
    
    msg->type = type;
    msg->data_size = size;
    memcpy(msg->data, data, size);
    
    // Send pointer to message
    bool result = (xQueueSend(queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE);
    
    if(!result) {
        // Queue send failed - clean up
        vPortFree(msg);
    }
    
    return result;
}
```

### Memory Statistics Dashboard
Real-time heap monitoring displayed in system dashboard:
- **Heap utilization**: Percentage of heap used
- **Fragmentation level**: Calculated fragmentation percentage
- **Peak usage**: Minimum free heap ever recorded
- **Allocation efficiency**: Success/failure ratios

## Common Pitfalls

### Memory Management Errors
- **Memory leaks**: Forgetting to free allocated memory
- **Double free**: Calling vPortFree twice on same pointer
- **Use after free**: Accessing memory after it's been freed
- **Buffer overruns**: Writing beyond allocated boundaries

### Design Issues
- **Excessive fragmentation**: Poor allocation patterns
- **Over-allocation**: Allocating more than needed
- **No error handling**: Not checking for allocation failures
- **Mixed allocation**: Using both malloc and pvPortMalloc

## Best Practices
- **Check allocation results**: Always verify pvPortMalloc success
- **Free in reverse order**: LIFO freeing reduces fragmentation
- **Use memory pools**: For fixed-size, frequent allocations
- **Monitor heap health**: Track fragmentation and usage
- **Design for failure**: Handle out-of-memory conditions gracefully
- **Prefer static allocation**: When memory requirements are known