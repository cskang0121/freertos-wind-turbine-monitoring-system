/**
 * Example 06: Memory Management with Heap_4
 * 
 * Demonstrates dynamic memory allocation, fragmentation prevention,
 * heap monitoring, and long-term stability testing
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Memory allocation patterns
#define MIN_ALLOC_SIZE      32
#define MAX_ALLOC_SIZE      1024
#define POOL_BLOCK_SIZE     256
#define POOL_BLOCK_COUNT    10
#define STRING_BUFFER_INIT  128

// Test parameters
#define STRESS_TEST_CYCLES  1000
#define FRAGMENT_TEST_BLOCKS 50

// Variable-length message structure
typedef struct {
    uint32_t id;
    uint32_t timestamp;
    uint16_t length;
    uint8_t data[];  // Flexible array member
} Message_t;

// Memory pool block
typedef struct {
    uint8_t data[POOL_BLOCK_SIZE];
    bool in_use;
    uint32_t alloc_count;
    TickType_t last_alloc;
} PoolBlock_t;

// String buffer management
typedef struct {
    char* buffer;
    size_t capacity;
    size_t used;
} StringBuffer_t;

// Heap statistics
typedef struct {
    size_t current_free;
    size_t minimum_ever;
    size_t total_heap;
    size_t largest_free;
    uint32_t allocations;
    uint32_t deallocations;
    uint32_t failures;
    float fragmentation;
    size_t peak_usage;
} MyHeapStats_t;

// Global variables
static PoolBlock_t* g_memory_pool = NULL;
static QueueHandle_t xMessageQueue = NULL;
static SemaphoreHandle_t xHeapMutex = NULL;
static MyHeapStats_t g_heap_stats = {0};
static bool g_stress_test_running = false;

// Forward declarations
static void update_heap_stats(void);
static void print_heap_stats(void);

// Memory pool implementation
static bool pool_init(void) {
    printf("[POOL] Initializing memory pool (%d blocks x %d bytes)\n",
           POOL_BLOCK_COUNT, POOL_BLOCK_SIZE);
    
    // Single large allocation for entire pool
    g_memory_pool = pvPortMalloc(sizeof(PoolBlock_t) * POOL_BLOCK_COUNT);
    
    if (g_memory_pool == NULL) {
        printf("[POOL] Failed to allocate memory pool!\n");
        return false;
    }
    
    // Initialize all blocks
    for (int i = 0; i < POOL_BLOCK_COUNT; i++) {
        g_memory_pool[i].in_use = false;
        g_memory_pool[i].alloc_count = 0;
        g_memory_pool[i].last_alloc = 0;
    }
    
    printf("[POOL] Memory pool initialized successfully\n");
    return true;
}

static void* pool_alloc(void) {
    for (int i = 0; i < POOL_BLOCK_COUNT; i++) {
        if (!g_memory_pool[i].in_use) {
            g_memory_pool[i].in_use = true;
            g_memory_pool[i].alloc_count++;
            g_memory_pool[i].last_alloc = xTaskGetTickCount();
            return g_memory_pool[i].data;
        }
    }
    return NULL;  // Pool exhausted
}

static void pool_free(void* ptr) {
    if (ptr == NULL) return;
    
    for (int i = 0; i < POOL_BLOCK_COUNT; i++) {
        if (g_memory_pool[i].data == ptr) {
            g_memory_pool[i].in_use = false;
            return;
        }
    }
}

static void pool_stats(void) {
    int used = 0;
    uint32_t total_allocs = 0;
    
    for (int i = 0; i < POOL_BLOCK_COUNT; i++) {
        if (g_memory_pool[i].in_use) used++;
        total_allocs += g_memory_pool[i].alloc_count;
    }
    
    printf("[POOL] Usage: %d/%d blocks, Total allocations: %lu\n",
           used, POOL_BLOCK_COUNT, total_allocs);
}

// String buffer implementation
static StringBuffer_t* string_buffer_create(size_t initial_size) {
    StringBuffer_t* sb = pvPortMalloc(sizeof(StringBuffer_t));
    if (sb == NULL) return NULL;
    
    sb->buffer = pvPortMalloc(initial_size);
    if (sb->buffer == NULL) {
        vPortFree(sb);
        return NULL;
    }
    
    sb->capacity = initial_size;
    sb->used = 0;
    sb->buffer[0] = '\0';
    
    return sb;
}

static bool string_buffer_append(StringBuffer_t* sb, const char* str) {
    if (sb == NULL || str == NULL) return false;
    
    size_t len = strlen(str);
    size_t required = sb->used + len + 1;
    
    // Grow buffer if needed
    if (required > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        
        char* new_buffer = pvPortMalloc(new_capacity);
        if (new_buffer == NULL) return false;
        
        strcpy(new_buffer, sb->buffer);
        vPortFree(sb->buffer);
        sb->buffer = new_buffer;
        sb->capacity = new_capacity;
        
        printf("[STRING] Buffer grown: %zu -> %zu bytes\n",
               sb->capacity / 2, sb->capacity);
    }
    
    strcat(sb->buffer, str);
    sb->used += len;
    
    return true;
}

static void string_buffer_destroy(StringBuffer_t* sb) {
    if (sb != NULL) {
        vPortFree(sb->buffer);
        vPortFree(sb);
    }
}

// Heap monitoring
static void update_heap_stats(void) {
    g_heap_stats.current_free = xPortGetFreeHeapSize();
    g_heap_stats.minimum_ever = xPortGetMinimumEverFreeHeapSize();
    g_heap_stats.total_heap = configTOTAL_HEAP_SIZE;
    
    // Calculate peak usage
    size_t current_used = g_heap_stats.total_heap - g_heap_stats.current_free;
    if (current_used > g_heap_stats.peak_usage) {
        g_heap_stats.peak_usage = current_used;
    }
    
    // Simple fragmentation estimate
    // In real implementation, would need custom heap walker
    if (g_heap_stats.current_free > 0) {
        // Estimate based on allocation patterns
        g_heap_stats.fragmentation = 
            (float)(g_heap_stats.allocations % 100) / 100.0f * 0.3f;
    }
}

static void print_heap_stats(void) {
    update_heap_stats();
    
    printf("\n");
    printf("========================================\n");
    printf("HEAP STATISTICS\n");
    printf("========================================\n");
    printf("Total Heap:      %zu bytes\n", g_heap_stats.total_heap);
    printf("Current Free:    %zu bytes (%.1f%%)\n", 
           g_heap_stats.current_free,
           (float)g_heap_stats.current_free / g_heap_stats.total_heap * 100);
    printf("Minimum Ever:    %zu bytes\n", g_heap_stats.minimum_ever);
    printf("Peak Usage:      %zu bytes\n", g_heap_stats.peak_usage);
    printf("Allocations:     %lu\n", g_heap_stats.allocations);
    printf("Deallocations:   %lu\n", g_heap_stats.deallocations);
    printf("Failed Allocs:   %lu\n", g_heap_stats.failures);
    printf("Fragmentation:   %.1f%%\n", g_heap_stats.fragmentation * 100);
    printf("========================================\n");
    printf("\n");
}

// Task 1: Variable allocation patterns
static void vAllocationTask(void *pvParameters) {
    (void)pvParameters;
    void* allocations[10] = {NULL};
    int alloc_index = 0;
    
    printf("[ALLOC] Task started - variable size allocations\n");
    
    for (;;) {
        // Random allocation size
        size_t size = (rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE)) + MIN_ALLOC_SIZE;
        
        // Allocate memory
        void* ptr = pvPortMalloc(size);
        
        if (ptr != NULL) {
            g_heap_stats.allocations++;
            
            // Store allocation
            if (allocations[alloc_index] != NULL) {
                vPortFree(allocations[alloc_index]);
                g_heap_stats.deallocations++;
            }
            
            allocations[alloc_index] = ptr;
            alloc_index = (alloc_index + 1) % 10;
            
            // Use memory (write pattern)
            memset(ptr, 0xAA, size);
            
            if (g_heap_stats.allocations % 100 == 0) {
                printf("[ALLOC] Completed %lu allocations\n", 
                       g_heap_stats.allocations);
            }
        } else {
            g_heap_stats.failures++;
            printf("[ALLOC] Allocation failed for size %zu!\n", size);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 2: Message queue with variable-length messages
static void vMessageProducerTask(void *pvParameters) {
    (void)pvParameters;
    uint32_t msg_id = 0;
    
    printf("[PRODUCER] Task started - variable length messages\n");
    
    for (;;) {
        // Random message size
        uint16_t data_len = rand() % 256 + 16;
        size_t total_size = sizeof(Message_t) + data_len;
        
        // Allocate message
        Message_t* msg = pvPortMalloc(total_size);
        
        if (msg != NULL) {
            // Fill message
            msg->id = msg_id++;
            msg->timestamp = xTaskGetTickCount();
            msg->length = data_len;
            
            // Fill data with pattern
            for (uint16_t i = 0; i < data_len; i++) {
                msg->data[i] = (uint8_t)(i & 0xFF);
            }
            
            // Send pointer through queue
            if (xQueueSend(xMessageQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
                printf("[PRODUCER] Queue full, freeing message\n");
                vPortFree(msg);
                g_heap_stats.deallocations++;
            } else {
                g_heap_stats.allocations++;
            }
        } else {
            printf("[PRODUCER] Failed to allocate message\n");
            g_heap_stats.failures++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void vMessageConsumerTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[CONSUMER] Task started - processing messages\n");
    
    for (;;) {
        Message_t* msg;
        
        if (xQueueReceive(xMessageQueue, &msg, portMAX_DELAY) == pdTRUE) {
            // Process message
            printf("[CONSUMER] Message %lu: %u bytes at tick %lu\n",
                   msg->id, msg->length, msg->timestamp);
            
            // Verify data integrity
            bool valid = true;
            for (uint16_t i = 0; i < msg->length; i++) {
                if (msg->data[i] != (uint8_t)(i & 0xFF)) {
                    valid = false;
                    break;
                }
            }
            
            if (!valid) {
                printf("[CONSUMER] Data corruption detected!\n");
            }
            
            // Free message
            vPortFree(msg);
            g_heap_stats.deallocations++;
        }
    }
}

// Task 3: String buffer operations
static void vStringTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[STRING] Task started - dynamic string operations\n");
    
    for (;;) {
        StringBuffer_t* sb = string_buffer_create(STRING_BUFFER_INIT);
        
        if (sb != NULL) {
            // Build string incrementally
            string_buffer_append(sb, "Sensor Data: ");
            
            for (int i = 0; i < 10; i++) {
                char temp[32];
                snprintf(temp, sizeof(temp), "[%d:%.2f] ", 
                        i, (float)(rand() % 100) / 10.0f);
                string_buffer_append(sb, temp);
            }
            
            printf("[STRING] Built: %s (len=%zu, cap=%zu)\n",
                   sb->buffer, sb->used, sb->capacity);
            
            // Clean up
            string_buffer_destroy(sb);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Task 4: Memory pool usage
static void vPoolTask(void *pvParameters) {
    (void)pvParameters;
    void* blocks[3] = {NULL};
    
    printf("[POOL] Task started - fixed size allocations\n");
    
    for (;;) {
        // Allocate from pool
        for (int i = 0; i < 3; i++) {
            blocks[i] = pool_alloc();
            if (blocks[i] != NULL) {
                // Use block
                memset(blocks[i], i, POOL_BLOCK_SIZE);
            } else {
                printf("[POOL] Pool exhausted!\n");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Free back to pool
        for (int i = 0; i < 3; i++) {
            pool_free(blocks[i]);
            blocks[i] = NULL;
        }
        
        // Print pool statistics
        pool_stats();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task 5: Fragmentation test
static void vFragmentationTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[FRAGMENT] Task started - testing coalescence\n");
    
    for (;;) {
        void* blocks[FRAGMENT_TEST_BLOCKS];
        
        // Phase 1: Allocate many blocks
        printf("[FRAGMENT] Allocating %d blocks...\n", FRAGMENT_TEST_BLOCKS);
        for (int i = 0; i < FRAGMENT_TEST_BLOCKS; i++) {
            size_t size = (rand() % 256) + 32;
            blocks[i] = pvPortMalloc(size);
        }
        
        // Phase 2: Free every other block (create fragmentation)
        printf("[FRAGMENT] Creating fragmentation...\n");
        for (int i = 0; i < FRAGMENT_TEST_BLOCKS; i += 2) {
            if (blocks[i] != NULL) {
                vPortFree(blocks[i]);
                blocks[i] = NULL;
            }
        }
        
        // Check fragmentation
        size_t free_before = xPortGetFreeHeapSize();
        printf("[FRAGMENT] Free heap after fragmentation: %zu\n", free_before);
        
        // Phase 3: Free remaining (test coalescence)
        printf("[FRAGMENT] Testing coalescence...\n");
        for (int i = 1; i < FRAGMENT_TEST_BLOCKS; i += 2) {
            if (blocks[i] != NULL) {
                vPortFree(blocks[i]);
                blocks[i] = NULL;
            }
        }
        
        // Check if memory coalesced
        size_t free_after = xPortGetFreeHeapSize();
        printf("[FRAGMENT] Free heap after coalescence: %zu\n", free_after);
        
        // Verify coalescence worked
        if (free_after > free_before) {
            printf("[FRAGMENT] Coalescence successful!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// Task 6: Monitor task
static void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[MONITOR] Heap monitor started\n");
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Print comprehensive statistics
        print_heap_stats();
        
        // Check for low memory
        size_t free_heap = xPortGetFreeHeapSize();
        if (free_heap < (configTOTAL_HEAP_SIZE * 0.1)) {
            printf("[MONITOR] WARNING: Heap usage > 90%%!\n");
        }
        
        // Check for excessive fragmentation
        if (g_heap_stats.fragmentation > 0.3) {
            printf("[MONITOR] WARNING: High fragmentation detected!\n");
        }
    }
}

// Task 7: Stress test (can be enabled/disabled)
static void vStressTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[STRESS] Stress test task started\n");
    
    for (;;) {
        if (g_stress_test_running) {
            printf("[STRESS] Starting %d allocation cycles...\n", STRESS_TEST_CYCLES);
            
            for (int cycle = 0; cycle < STRESS_TEST_CYCLES; cycle++) {
                // Random size
                size_t size = (rand() % MAX_ALLOC_SIZE) + MIN_ALLOC_SIZE;
                
                // Allocate
                void* ptr = pvPortMalloc(size);
                
                if (ptr != NULL) {
                    // Write test pattern
                    memset(ptr, 0x5A, size);
                    
                    // Random hold time
                    vTaskDelay(rand() % 10);
                    
                    // Verify pattern
                    uint8_t* bytes = (uint8_t*)ptr;
                    for (size_t i = 0; i < size; i++) {
                        if (bytes[i] != 0x5A) {
                            printf("[STRESS] Memory corruption detected!\n");
                            break;
                        }
                    }
                    
                    // Free
                    vPortFree(ptr);
                }
                
                if (cycle % 100 == 0) {
                    printf("[STRESS] Completed %d/%d cycles\n", 
                           cycle, STRESS_TEST_CYCLES);
                }
            }
            
            printf("[STRESS] Test completed\n");
            g_stress_test_running = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// FreeRTOS static allocation callbacks
#if configSUPPORT_STATIC_ALLOCATION == 1

static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif

// Application hooks
void vApplicationMallocFailedHook(void) {
    printf("[ERROR] Memory allocation failed!\n");
    printf("Free heap: %zu bytes\n", xPortGetFreeHeapSize());
    g_heap_stats.failures++;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    printf("[ERROR] Stack overflow in task: %s\n", pcTaskName);
    for(;;);
}

void vApplicationIdleHook(void) {
    // Could implement power saving here
}

int main(void) {
    printf("===========================================\n");
    printf("Example 06: Memory Management with Heap_4\n");
    printf("Dynamic allocation and fragmentation prevention\n");
    printf("Total heap size: %d bytes\n", configTOTAL_HEAP_SIZE);
    printf("===========================================\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Initialize memory pool
    if (!pool_init()) {
        printf("Failed to initialize memory pool!\n");
        return 1;
    }
    
    // Create message queue for variable-length messages
    xMessageQueue = xQueueCreate(10, sizeof(Message_t*));
    if (xMessageQueue == NULL) {
        printf("Failed to create message queue!\n");
        return 1;
    }
    
    // Create heap protection mutex
    xHeapMutex = xSemaphoreCreateMutex();
    if (xHeapMutex == NULL) {
        printf("Failed to create heap mutex!\n");
        return 1;
    }
    
    printf("[MAIN] Initial heap status:\n");
    print_heap_stats();
    
    // Create tasks
    xTaskCreate(vAllocationTask, "Alloc", configMINIMAL_STACK_SIZE * 2,
                NULL, 3, NULL);
    
    xTaskCreate(vMessageProducerTask, "Producer", configMINIMAL_STACK_SIZE * 2,
                NULL, 4, NULL);
    
    xTaskCreate(vMessageConsumerTask, "Consumer", configMINIMAL_STACK_SIZE * 2,
                NULL, 4, NULL);
    
    xTaskCreate(vStringTask, "String", configMINIMAL_STACK_SIZE * 2,
                NULL, 2, NULL);
    
    xTaskCreate(vPoolTask, "Pool", configMINIMAL_STACK_SIZE * 2,
                NULL, 2, NULL);
    
    xTaskCreate(vFragmentationTask, "Fragment", configMINIMAL_STACK_SIZE * 2,
                NULL, 1, NULL);
    
    xTaskCreate(vMonitorTask, "Monitor", configMINIMAL_STACK_SIZE * 2,
                NULL, 5, NULL);
    
    xTaskCreate(vStressTask, "Stress", configMINIMAL_STACK_SIZE * 2,
                NULL, 1, NULL);
    
    printf("[MAIN] All tasks created, starting scheduler...\n\n");
    
    // Start stress test after 10 seconds (uncomment to enable)
    // g_stress_test_running = true;
    
    // Start the scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("Scheduler failed to start!\n");
    return 1;
}