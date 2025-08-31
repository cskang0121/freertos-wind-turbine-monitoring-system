# ISR Handling with Deferred Processing - Quick Reference

## Overview
This capability demonstrates interrupt service routines (ISRs) with proper deferred processing patterns. ISRs must be minimal and fast, with actual work deferred to high-priority tasks.

**Key Concepts:**
- Minimal ISR execution time
- Deferred processing using tasks
- FromISR API variants for thread safety
- Context switching from interrupt context
- ISR-to-task communication via queues/semaphores

## Core ISR Principles

### 1. ISR Design Rules
- **Keep ISRs minimal**: < 100 instructions ideal
- **No blocking operations**: Never wait in ISR
- **Fast execution**: Complete within microseconds
- **Defer heavy work**: Signal tasks for processing
- **Thread-safe APIs**: Use FromISR variants

### 2. Hardware Interrupt Flow
```
Physical Event → Hardware Interrupt Controller → CPU Vector Table → ISR Function
```

### 3. ISR to Task Communication
```
[Timer ISR] --signal--> [High Priority Task] --process--> [System State]
```

## Core FreeRTOS ISR APIs

### ISR-Safe Queue Operations
```c
// Send data from ISR
BaseType_t xQueueSendFromISR(
    QueueHandle_t xQueue,
    const void *pvItemToQueue,
    BaseType_t *pxHigherPriorityTaskWoken
);

// Receive in task (not ISR)
BaseType_t xQueueReceive(
    QueueHandle_t xQueue,
    void *pvBuffer,
    TickType_t xTicksToWait
);
```

### ISR-Safe Semaphore Operations
```c
// Give semaphore from ISR
BaseType_t xSemaphoreGiveFromISR(
    SemaphoreHandle_t xSemaphore,
    BaseType_t *pxHigherPriorityTaskWoken
);

// Context switch if needed
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

## Implementation Patterns

### 1. Timer-Based ISR Pattern
```c
// ISR function - keep minimal!
void TIMER_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Clear interrupt flag
    TIMER_ClearInterrupt();
    
    // Signal processing task
    xSemaphoreGiveFromISR(xProcessingSemaphore, &xHigherPriorityTaskWoken);
    
    // Context switch if higher priority task woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### 2. Deferred Processing Task
```c
void vProcessingTask(void *pvParameters) {
    while(1) {
        // Wait for ISR signal
        if(xSemaphoreTake(xProcessingSemaphore, portMAX_DELAY) == pdTRUE) {
            // Do the actual work here
            process_sensor_data();
            update_system_state();
            
            // Record processing time
            calculate_isr_latency();
        }
    }
}
```

### 3. GPIO Interrupt Pattern
```c
void EXTI0_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(GPIO_GetInterruptStatus(BUTTON_PIN)) {
        GPIO_ClearInterrupt(BUTTON_PIN);
        
        // Send button event to queue
        ButtonEvent_t event = {BUTTON_PRESSED, xTaskGetTickCountFromISR()};
        xQueueSendFromISR(xButtonQueue, &event, &xHigherPriorityTaskWoken);
        
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

## Performance Considerations

### ISR Latency Metrics
- **Interrupt Response**: < 10μs (hardware dependent)
- **ISR Execution Time**: < 100μs (keep minimal)
- **ISR-to-Task Latency**: < 1ms (deferred processing)
- **Context Switch Time**: < 50μs (scheduler overhead)

### Optimization Tips
- **Minimize ISR code**: Only essential operations
- **Use DMA**: Reduce CPU involvement in data transfers
- **Batch processing**: Process multiple items per task wake-up
- **Priority tuning**: Set processing task priority appropriately

## Common Pitfalls

### What NOT to do in ISRs
- **Blocking calls**: Never use vTaskDelay, mutex take, etc.
- **Heavy computation**: No complex algorithms
- **Standard APIs**: Use FromISR variants only
- **printf/logging**: Avoid in production ISRs
- **Long loops**: Keep execution predictable

### Thread Safety Issues
- **Missing FromISR**: Using regular FreeRTOS APIs in ISR
- **Forgotten yield**: Not calling portYIELD_FROM_ISR
- **Race conditions**: Unprotected shared data access

## Integration with Wind Turbine System

### Sensor Sampling ISR (100Hz)
Our system uses a timer ISR to sample vibration sensors at 100Hz:

```c
void vTimer100HzISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Read sensor (fast hardware operation)
    SensorReading_t reading;
    reading.vibration = ADC_ReadVibration();
    reading.timestamp = xTaskGetTickCountFromISR();
    
    // Send to processing queue
    xQueueSendFromISR(xSensorISRQueue, &reading, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Deferred Processing Task
The sensor task processes ISR data with proper error handling:
- **Priority 4**: High enough for real-time response
- **Stack size**: 1KB for processing algorithms
- **Queue monitoring**: Tracks ISR-to-task latency
- **Error handling**: Manages queue overflow conditions

## Best Practices
- **Design for determinism**: Predictable ISR execution times
- **Use appropriate priorities**: Balance responsiveness vs CPU usage
- **Monitor performance**: Track latency and CPU utilization
- **Plan for worst case**: Size queues for burst conditions
- **Test thoroughly**: Verify ISR behavior under load