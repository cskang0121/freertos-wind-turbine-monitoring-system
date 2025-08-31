# Queue-Based Producer-Consumer - Quick Reference

## Overview
This capability demonstrates inter-task communication using FreeRTOS queues. Queues enable safe data transfer between tasks running at different priorities and frequencies.

**Key Concepts:**
- Thread-safe inter-task communication
- FIFO (First In, First Out) data ordering
- Blocking and non-blocking queue operations
- Queue sizing and overflow handling
- Producer-consumer patterns

## Core Queue Operations

### 1. Queue Creation
```c
QueueHandle_t xQueueCreate(
    UBaseType_t uxQueueLength,    // Number of items queue can hold
    UBaseType_t uxItemSize        // Size of each item in bytes
);
```

### 2. Sending Data (Producer)
```c
// Blocking send (wait if queue full)
BaseType_t xQueueSend(
    QueueHandle_t xQueue,
    const void *pvItemToQueue,
    TickType_t xTicksToWait
);

// Non-blocking send (fail immediately if full)
BaseType_t xQueueSend(xQueue, &data, 0);
```

### 3. Receiving Data (Consumer)  
```c
// Blocking receive (wait if queue empty)
BaseType_t xQueueReceive(
    QueueHandle_t xQueue,
    void *pvBuffer,
    TickType_t xTicksToWait
);

// Non-blocking receive
BaseType_t xQueueReceive(xQueue, &buffer, 0);
```

## Queue Design Patterns

### 1. High Frequency Producer → Lower Frequency Consumer
```
Sensor Task (100Hz) --[Queue Size: 10]--> Processing Task (10Hz)
```
- **Queue sizing**: Rate × Latency × Safety Factor
- **Example**: 100Hz × 0.1s × 2 = 20 items minimum

### 2. Multiple Producers → Single Consumer
```
Fast Producer (100Hz) ┐
Medium Producer (10Hz) ├--[Queue]--> Consumer Task
Burst Producer (event) ┘
```

### 3. Single Producer → Multiple Consumers
```
                    ┌--> High Priority Consumer
Data Producer --[Q1]├--> Medium Priority Consumer  
                    └--> Low Priority Consumer
```

## Implementation Patterns

### Basic Producer Task
```c
void vProducerTask(void *pvParameters) {
    SensorData_t sensorReading;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        // Generate/collect data
        sensorReading.vibration = read_vibration_sensor();
        sensorReading.temperature = read_temperature_sensor();
        sensorReading.timestamp = xTaskGetTickCount();
        
        // Send to queue (block up to 100ms if full)
        if(xQueueSend(xSensorQueue, &sensorReading, pdMS_TO_TICKS(100)) != pdTRUE) {
            // Handle queue full condition
            queue_overflow_count++;
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

### Basic Consumer Task
```c
void vConsumerTask(void *pvParameters) {
    SensorData_t receivedData;
    
    while(1) {
        // Wait for data (block indefinitely)
        if(xQueueReceive(xSensorQueue, &receivedData, portMAX_DELAY) == pdTRUE) {
            // Process the received data
            process_sensor_data(&receivedData);
            
            // Update statistics
            items_processed++;
            
            // Check processing latency
            TickType_t latency = xTaskGetTickCount() - receivedData.timestamp;
            if(latency > max_latency) max_latency = latency;
        }
    }
}
```

## Queue Monitoring & Debugging

### Queue Status Information
```c
// Get current queue status
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t xQueue);

// Usage example
UBaseType_t used = uxQueueMessagesWaiting(xSensorQueue);
UBaseType_t free = uxQueueSpacesAvailable(xSensorQueue);
float utilization = (float)used / (used + free) * 100.0f;
```

### Performance Metrics
- **Queue Utilization**: Percentage of queue slots used
- **Overflow Count**: Number of failed sends
- **Processing Latency**: Time from send to receive
- **Throughput**: Items processed per second

## Queue Sizing Strategy

### Sizing Formula
```
Queue Size = (Producer Rate × Max Processing Latency × Safety Factor)
```

### Examples
- **Sensor Data**: 100Hz × 0.1s × 2 = 20 items
- **Network Packets**: 10Hz × 0.5s × 3 = 15 items  
- **Log Messages**: 50Hz × 0.2s × 2 = 20 items

### Considerations
- **Memory usage**: Larger queues consume more RAM
- **Latency vs throughput**: Balance responsiveness with efficiency
- **Burst handling**: Size for worst-case scenarios
- **Producer blocking**: Avoid critical tasks blocking on full queues

## Common Pitfalls

### Queue Design Issues
- **Undersized queues**: Frequent overflows under load
- **Oversized queues**: Excessive memory usage
- **Wrong timeout values**: Tasks blocking unexpectedly
- **Mixed data types**: Type safety issues

### Performance Problems  
- **Priority inversion**: Low priority consumer blocking high priority producer
- **Starvation**: Consumer too slow, queue constantly full
- **Memory leaks**: Not freeing dynamically allocated queue items
- **Deadlock**: Circular queue dependencies

## Integration with Wind Turbine System

### Multi-Queue Architecture
Our system uses multiple specialized queues:

```c
// Sensor data pipeline
QueueHandle_t xSensorDataQueue;    // Raw sensor readings (10 items)
QueueHandle_t xProcessedQueue;     // Processed data (5 items)  
QueueHandle_t xAnomalyQueue;       // Anomaly alerts (3 items)
QueueHandle_t xNetworkQueue;       // Network packets (5 items)
```

### Queue Flow Design
```
Sensor Task (10Hz) --[SensorQueue]--> Anomaly Task (5Hz)
                                           |
                                    [AnomalyQueue]
                                           |
                                   --> Network Task (1Hz)
```

### Queue Monitoring Dashboard
The system displays real-time queue statistics:
- **Queue utilization bars**: Visual queue fill levels
- **Overflow counters**: Track data loss events  
- **Latency metrics**: End-to-end processing times
- **Efficiency ratings**: Successful vs dropped packets

## Best Practices
- **Design for peak load**: Size queues for burst conditions
- **Monitor queue health**: Track utilization and overflows
- **Use appropriate timeouts**: Balance blocking vs performance
- **Handle failures gracefully**: Always check return values
- **Consider memory constraints**: Optimize queue sizes for target platform
- **Test under load**: Verify behavior during stress conditions