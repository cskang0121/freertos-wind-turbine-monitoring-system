# Mutex for Shared Resources - Quick Reference

## Overview
This capability demonstrates protecting shared resources using mutexes (mutual exclusion). Mutexes prevent race conditions when multiple tasks access the same hardware or data structures.

**Key Concepts:**
- Resource ownership and mutual exclusion
- Priority inheritance to prevent priority inversion
- Recursive mutexes for nested function calls
- Deadlock prevention strategies
- Critical section design

## Mutex vs Semaphore Comparison

| Feature | Mutex | Binary Semaphore |
|---------|-------|------------------|
| **Purpose** | Resource protection | Event signaling |
| **Ownership** | Yes (tracks owner) | No |
| **Initial State** | Available (1) | Empty (0) |
| **Priority Inheritance** | Yes | No |
| **Recursive Taking** | Available | Not applicable |

## Core Mutex Operations

### 1. Mutex Creation
```c
// Standard mutex
SemaphoreHandle_t xSemaphoreCreateMutex(void);

// Recursive mutex (can be taken multiple times by owner)
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
```

### 2. Taking/Giving Mutex
```c
// Take mutex (acquire resource)
BaseType_t xSemaphoreTake(
    SemaphoreHandle_t xSemaphore,
    TickType_t xTicksToWait
);

// Give mutex (release resource)
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

// Recursive variants
BaseType_t xSemaphoreTakeRecursive(xMutex, xTicksToWait);
BaseType_t xSemaphoreGiveRecursive(xMutex);
```

## Implementation Patterns

### 1. Shared Hardware Protection
```c
// Global SPI mutex
SemaphoreHandle_t xSPIMutex = NULL;

// Thread-safe SPI wrapper
void vSafe_SPI_Transaction(uint8_t device_id, uint8_t* data, size_t len) {
    // Take mutex with timeout
    if(xSemaphoreTake(xSPIMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Critical section - exclusive SPI access
        SPI_SelectDevice(device_id);
        SPI_Transfer(data, len);
        SPI_DeselectDevice();
        
        // Always release mutex
        xSemaphoreGive(xSPIMutex);
    } else {
        // Handle timeout - mutex acquisition failed
        error_count++;
    }
}
```

### 2. Shared Data Structure Protection
```c
// Protected configuration structure
typedef struct {
    float sensor_threshold;
    uint32_t sample_rate;
    bool calibration_mode;
} SystemConfig_t;

SystemConfig_t g_config;
SemaphoreHandle_t xConfigMutex = NULL;

// Thread-safe configuration update
void vUpdateConfig(float threshold, uint32_t rate) {
    if(xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Atomic configuration update
        g_config.sensor_threshold = threshold;
        g_config.sample_rate = rate;
        g_config.calibration_mode = false;
        
        xSemaphoreGive(xConfigMutex);
    }
}
```

### 3. Recursive Mutex Pattern
```c
SemaphoreHandle_t xRecursiveMutex = NULL;

void vNestedFunction(void) {
    // Take mutex (may be taken multiple times by same task)
    if(xSemaphoreTakeRecursive(xRecursiveMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Do work
        vAnotherProtectedFunction(); // This may also take the same mutex
        
        // Must give exactly as many times as taken
        xSemaphoreGiveRecursive(xRecursiveMutex);
    }
}
```

## Priority Inheritance

### Problem: Priority Inversion
```
High Priority Task (blocked) ← waiting for mutex
Medium Priority Task (running) ← preempts low priority
Low Priority Task (owns mutex) ← cannot complete
```

### Solution: Priority Inheritance
```c
// When high priority task blocks on mutex:
// 1. Low priority task inherits high priority temporarily  
// 2. Low priority task completes critical section
// 3. Low priority task releases mutex and returns to original priority
// 4. High priority task acquires mutex and continues
```

### FreeRTOS Implementation
```c
// Priority inheritance is automatic with mutexes
// No special code required - FreeRTOS handles it internally
if(xSemaphoreTake(xMutex, timeout) == pdTRUE) {
    // If higher priority task blocks here, this task's priority 
    // is automatically boosted until mutex is released
    critical_section_work();
    xSemaphoreGive(xMutex);  // Priority restored automatically
}
```

## Deadlock Prevention

### Classic Deadlock Scenario
```c
// Task A:                    Task B:
xSemaphoreTake(mutex1);       xSemaphoreTake(mutex2);
xSemaphoreTake(mutex2);  ←→   xSemaphoreTake(mutex1);  // DEADLOCK!
```

### Prevention Strategies
1. **Lock Ordering**: Always acquire mutexes in same order
2. **Timeout Values**: Use timeouts instead of infinite wait
3. **Single Mutex**: Protect related resources with one mutex
4. **Lock Hierarchy**: Define mutex precedence levels

```c
// Good: Consistent lock ordering
void vSafeFunction(void) {
    if(xSemaphoreTake(xMutex1, timeout) == pdTRUE) {
        if(xSemaphoreTake(xMutex2, timeout) == pdTRUE) {
            // Critical section with both resources
            xSemaphoreGive(xMutex2);  // Release in reverse order
        }
        xSemaphoreGive(xMutex1);
    }
}
```

## Performance Considerations

### Mutex Statistics Tracking
```c
typedef struct {
    uint32_t takes;
    uint32_t gives;
    uint32_t timeouts;
    uint32_t max_hold_time;
} MutexStats_t;

MutexStats_t g_spi_stats = {0};

// Enhanced mutex operations with statistics
BaseType_t vSafe_TakeMutex(SemaphoreHandle_t mutex, TickType_t timeout) {
    TickType_t start_time = xTaskGetTickCount();
    
    if(xSemaphoreTake(mutex, timeout) == pdTRUE) {
        g_spi_stats.takes++;
        return pdTRUE;
    } else {
        g_spi_stats.timeouts++;
        return pdFALSE;
    }
}
```

### Critical Section Optimization
- **Minimize hold time**: Keep critical sections as short as possible
- **Prepare data first**: Do non-critical work before taking mutex
- **Batch operations**: Combine multiple operations in one critical section
- **Avoid blocking**: Never call vTaskDelay() while holding mutex

## Common Pitfalls

### Mutex Usage Errors
- **Forgetting to release**: Always pair take with give
- **Wrong task releasing**: Only the owning task can release mutex
- **Taking in ISR**: Never take mutex from interrupt context
- **Infinite timeout in critical tasks**: Can cause system stall

### Design Issues
- **Too many mutexes**: Increases complexity and deadlock risk
- **Too few mutexes**: Reduces parallelism and performance
- **Wrong granularity**: Protecting too much or too little
- **Nested locking**: Increases deadlock probability

## Integration with Wind Turbine System

### Multi-Sensor SPI Bus
Our system protects SPI bus access for multiple sensors:

```c
// Shared SPI bus protection
SemaphoreHandle_t xSPIMutex = NULL;

// Sensor reading functions
float vReadVibrationSensor(void) {
    float result = 0.0f;
    
    if(xSemaphoreTake(xSPIMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Exclusive SPI access
        SPI_Select_Vibration();
        result = SPI_Read_Float();
        SPI_Deselect();
        
        // Update mutex statistics
        g_system_state.mutex_stats.spi_mutex_takes++;
        xSemaphoreGive(xSPIMutex);
    } else {
        g_system_state.mutex_stats.spi_mutex_timeouts++;
    }
    
    return result;
}
```

### System State Protection
Multiple tasks safely access shared system state:
- **Sensor Task**: Updates sensor readings
- **Anomaly Task**: Reads sensor data and updates health scores
- **Safety Task**: Reads all data for emergency decisions
- **Dashboard Task**: Reads data for display

### Mutex Hierarchy
1. **System State Mutex**: Highest priority - protects critical system data
2. **Threshold Configuration Mutex**: Medium priority - protects settings
3. **SPI Bus Mutex**: Hardware level - protects peripheral access

## Best Practices
- **Always use timeouts**: Avoid infinite blocking
- **Keep critical sections short**: Minimize mutex hold time
- **Document lock order**: Prevent deadlock through consistent ordering
- **Monitor mutex health**: Track timeouts and hold times
- **Design for failure**: Handle mutex acquisition failures gracefully
- **Test under load**: Verify no deadlocks under stress conditions