# Capability 3: Queue-Based Producer-Consumer Pattern

## Overview

Queues are the primary inter-task communication mechanism in FreeRTOS. They provide thread-safe, buffered data transfer between tasks, essential for building robust data pipelines in our wind turbine system.

## Core Concepts

### 1. Queue Fundamentals

A queue is a FIFO (First In, First Out) data structure that:
- Holds a fixed number of fixed-size items
- Provides thread-safe access (no mutex needed)
- Can block tasks when full (producer) or empty (consumer)
- Copies data (by value, not reference)

```c
// Create a queue holding 10 items of sensor_data_t
QueueHandle_t xQueue = xQueueCreate(10, sizeof(sensor_data_t));
```

### 2. Producer-Consumer Pattern

```
Producers                    Queue                    Consumers
---------                    -----                    ---------
Sensor Task 1 ──┐                                 ┌──> Processing Task
                 ├──> [ ][ ][ ][ ][ ] ──>         │
Sensor Task 2 ──┤                                 ├──> Logging Task
                 │                                 │
Sensor Task 3 ──┘                                 └──> Network Task
```

### 3. Queue Operations

| Operation | Blocking | Non-Blocking | From ISR |
|-----------|----------|--------------|----------|
| Send | `xQueueSend()` | `xQueueSend(..., 0)` | `xQueueSendFromISR()` |
| Receive | `xQueueReceive()` | `xQueueReceive(..., 0)` | `xQueueReceiveFromISR()` |
| Peek | `xQueuePeek()` | `xQueuePeek(..., 0)` | `xQueuePeekFromISR()` |

### 4. Queue Sizing Strategy

```
Queue Size = (Producer Rate × Max Consumer Latency) × Safety Factor

Example:
- Producer: 100Hz (every 10ms)
- Consumer: Processes in 50ms worst case
- Safety Factor: 1.5

Queue Size = (100Hz × 0.05s) × 1.5 = 7.5 ≈ 8 items
```

## Wind Turbine Application

### Sensor Data Pipeline

```c
typedef struct {
    uint32_t timestamp;
    float temperature;
    float vibration;
    uint16_t rpm;
    uint8_t sensor_id;
} sensor_data_t;

// Main sensor queue
QueueHandle_t xSensorQueue = xQueueCreate(20, sizeof(sensor_data_t));

// Anomaly queue for AI processing
QueueHandle_t xAnomalyQueue = xQueueCreate(10, sizeof(sensor_data_t));

// Alert queue for network transmission
QueueHandle_t xAlertQueue = xQueueCreate(5, sizeof(alert_t));
```

## Implementation Patterns

### Pattern 1: Basic Producer

```c
void vProducerTask(void *pvParameters)
{
    sensor_data_t data;
    
    for (;;) {
        // Collect sensor data
        data.temperature = read_temperature();
        data.vibration = read_vibration();
        data.timestamp = xTaskGetTickCount();
        
        // Send with timeout (non-blocking)
        if (xQueueSend(xSensorQueue, &data, pdMS_TO_TICKS(10)) != pdTRUE) {
            // Queue full - handle overflow
            log_dropped_data();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz
    }
}
```

### Pattern 2: Basic Consumer

```c
void vConsumerTask(void *pvParameters)
{
    sensor_data_t data;
    
    for (;;) {
        // Block until data available
        if (xQueueReceive(xSensorQueue, &data, portMAX_DELAY) == pdTRUE) {
            // Process data
            analyze_sensor_data(&data);
            
            // Check for anomalies
            if (detect_anomaly(&data)) {
                // Forward to anomaly queue
                xQueueSend(xAnomalyQueue, &data, 0);
            }
        }
    }
}
```

### Pattern 3: Multiple Queue Monitor

```c
void vMultiQueueConsumer(void *pvParameters)
{
    QueueSetHandle_t xQueueSet;
    QueueSetMemberHandle_t xActivatedMember;
    
    // Create queue set
    xQueueSet = xQueueCreateSet(QUEUE1_LENGTH + QUEUE2_LENGTH);
    xQueueAddToSet(xQueue1, xQueueSet);
    xQueueAddToSet(xQueue2, xQueueSet);
    
    for (;;) {
        // Wait for any queue to have data
        xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
        
        if (xActivatedMember == xQueue1) {
            // Process Queue 1 data
        } else if (xActivatedMember == xQueue2) {
            // Process Queue 2 data
        }
    }
}
```

### Pattern 4: Priority Queue Processing

```c
void vPriorityConsumer(void *pvParameters)
{
    // Check high priority queue first
    if (xQueueReceive(xHighPriorityQueue, &data, 0) == pdTRUE) {
        process_high_priority(data);
    }
    // Then normal priority
    else if (xQueueReceive(xNormalQueue, &data, 0) == pdTRUE) {
        process_normal(data);
    }
    // Finally low priority
    else if (xQueueReceive(xLowQueue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
        process_low(data);
    }
}
```

## Advanced Features

### 1. Queue Peek
```c
// Look at next item without removing it
if (xQueuePeek(xQueue, &data, 0) == pdTRUE) {
    if (can_process(data)) {
        xQueueReceive(xQueue, &data, 0);  // Now remove it
        process(data);
    }
}
```

### 2. Queue Space Check
```c
// Check available space before producing
UBaseType_t spaces = uxQueueSpacesAvailable(xQueue);
if (spaces < 2) {
    // Queue almost full - slow down production
    increase_delay();
}
```

### 3. Queue Reset
```c
// Clear all items (careful - data is lost!)
xQueueReset(xQueue);
```

### 4. Send to Front (LIFO behavior)
```c
// High priority item - send to front
xQueueSendToFront(xQueue, &urgent_data, 0);
```

## Performance Optimization

### 1. Queue Size Optimization

| Queue Type | Size Strategy | Typical Size |
|------------|---------------|--------------|
| High-frequency sensor | 2× burst size | 20-50 items |
| Command queue | Small | 3-5 items |
| Log queue | Large buffer | 100+ items |
| Alert queue | Medium | 10-20 items |

### 2. Timeout Strategies

```c
// Producer timeouts
#define PRODUCER_TIMEOUT_MS  10    // Don't block long

// Consumer timeouts  
#define CONSUMER_TIMEOUT_MS  100   // Can wait longer

// Critical path - no wait
#define CRITICAL_TIMEOUT     0      // Non-blocking
```

### 3. Memory Considerations

```c
// Queue memory usage
size_t queue_memory = queue_length × (item_size + overhead);
// Overhead ≈ 8 bytes per item

// For 20 items of 32 bytes each:
// Memory = 20 × (32 + 8) = 800 bytes
```

## Common Pitfalls

### 1. Queue Full Handling
```c
// ❌ BAD: Ignore send failure
xQueueSend(xQueue, &data, 10);  // Return value ignored!

// ✅ GOOD: Handle failure
if (xQueueSend(xQueue, &data, 10) != pdTRUE) {
    // Queue full - log, drop, or retry
    handle_queue_full();
}
```

### 2. Blocking in Critical Section
```c
// ❌ BAD: Blocking send while holding mutex
xSemaphoreTake(xMutex, portMAX_DELAY);
xQueueSend(xQueue, &data, portMAX_DELAY);  // Can deadlock!
xSemaphoreGive(xMutex);

// ✅ GOOD: Non-blocking or release mutex first
xSemaphoreTake(xMutex, portMAX_DELAY);
prepare_data(&data);
xSemaphoreGive(xMutex);
xQueueSend(xQueue, &data, portMAX_DELAY);  // Safe to block
```

### 3. Size Mismatch
```c
// ❌ BAD: Wrong size
typedef struct { uint32_t a, b, c; } data_t;
xQueue = xQueueCreate(10, sizeof(uint32_t));  // Wrong size!
xQueueSend(xQueue, &data_struct, 0);          // Corruption!

// ✅ GOOD: Correct size
xQueue = xQueueCreate(10, sizeof(data_t));    // Correct
```

## Testing Strategies

### 1. Queue Utilization Test
```c
void monitor_queue_usage(QueueHandle_t xQueue)
{
    UBaseType_t items = uxQueueMessagesWaiting(xQueue);
    UBaseType_t spaces = uxQueueSpacesAvailable(xQueue);
    float utilization = (float)items / (items + spaces) * 100;
    
    printf("Queue utilization: %.1f%% (%u/%u)\n", 
           utilization, items, items + spaces);
}
```

### 2. Throughput Test
```c
uint32_t sent = 0, received = 0;

// Producer increments sent
// Consumer increments received
// Monitor calculates throughput

float throughput = (float)received / time_elapsed;
printf("Throughput: %.1f items/sec\n", throughput);
```

### 3. Latency Test
```c
typedef struct {
    uint32_t data;
    TickType_t timestamp;
} timestamped_data_t;

// Producer adds timestamp
// Consumer measures latency
TickType_t latency = xTaskGetTickCount() - data.timestamp;
```

## Best Practices

### DO's
✅ Size queues based on data rate analysis
✅ Handle queue full/empty conditions
✅ Use appropriate timeouts
✅ Monitor queue utilization
✅ Use queue sets for multiple queues
✅ Clear queues on task restart

### DON'Ts
❌ Ignore return values
❌ Use infinite timeouts everywhere
❌ Pass pointers through queues
❌ Block while holding resources
❌ Assume queue operations always succeed

## Learning Objectives

After completing this capability, you should be able to:
- [ ] Design appropriate queue sizes
- [ ] Implement producer-consumer patterns
- [ ] Handle queue overflow gracefully
- [ ] Use queue sets for multiple queues
- [ ] Optimize queue performance
- [ ] Debug queue-related issues

## Code Example

See `examples/03_producer_consumer/` for a complete demonstration including:
- Multiple producers at different rates
- Multiple consumers with priorities
- Queue utilization monitoring
- Overflow handling strategies
- Performance statistics

## Next Steps

After mastering queues, proceed to:
- Capability 4: Mutex for shared resource protection
- Combine queues with mutexes for complex synchronization
- Build complete data processing pipelines