# Capability 4: Mutex for Shared Resources

## Overview

Mutexes (Mutual Exclusion) are synchronization primitives that protect shared resources from concurrent access. In embedded systems, multiple tasks often need to access the same hardware peripherals (SPI, I2C, UART) or shared data structures. Without proper protection, race conditions can corrupt data or cause system failures.

## Core Concepts

### 1. Race Conditions

When multiple tasks access shared resources without synchronization:

```
Task A                  Task B                  Result
------                  ------                  ------
Read value (5)                                  
                        Read value (5)
Increment (6)                                   
                        Increment (6)
Write back (6)                                  
                        Write back (6)          Lost update! Should be 7
```

### 2. Mutex Operation

```
Task A                  Task B                  Resource State
------                  ------                  --------------
xSemaphoreTake()                                Locked by A
Access resource                                 
                        xSemaphoreTake()        B blocks (waits)
                        (blocked...)            
xSemaphoreGive()                                Unlocked
                        (unblocked)             Locked by B
                        Access resource
                        xSemaphoreGive()        Unlocked
```

### 3. Priority Inheritance

Prevents priority inversion where a high-priority task waits for a low-priority task:

```
Without Priority Inheritance:
High Priority Task (7) --> Blocked on mutex
Medium Priority Task (5) --> Runs (bad!)
Low Priority Task (2) --> Holds mutex

With Priority Inheritance:
High Priority Task (7) --> Blocked on mutex
Low Priority Task (2->7) --> Temporarily boosted to priority 7
Medium Priority Task (5) --> Cannot preempt (good!)
```

## FreeRTOS Mutex APIs

### Creating a Mutex

```c
// Standard mutex
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// Recursive mutex (same task can take multiple times)
SemaphoreHandle_t xRecursiveMutex = xSemaphoreCreateRecursiveMutex();

// Static allocation
StaticSemaphore_t xMutexBuffer;
SemaphoreHandle_t xMutex = xSemaphoreCreateMutexStatic(&xMutexBuffer);
```

### Taking a Mutex

```c
// Standard mutex with timeout
if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Critical section - access shared resource
    access_spi_bus();
    
    // Must give back mutex
    xSemaphoreGive(xMutex);
} else {
    // Timeout - handle error
    printf("Failed to acquire mutex\n");
}

// Recursive mutex
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
// Can take again from same task
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
// Must give back same number of times
xSemaphoreGiveRecursive(xRecursiveMutex);
xSemaphoreGiveRecursive(xRecursiveMutex);
```

### Mutex from ISR

**IMPORTANT**: Regular mutexes CANNOT be used from ISRs!

```c
// WRONG - Will cause system failure
void vISR_Handler(void) {
    xSemaphoreTake(xMutex, 0);  // NEVER do this!
}

// CORRECT - Use binary semaphore for ISR synchronization
void vISR_Handler(void) {
    xSemaphoreGiveFromISR(xBinarySemaphore, &xHigherPriorityTaskWoken);
}
```

## Implementation Pattern

### 1. Shared SPI Bus Example

```c
// Global mutex for SPI bus
static SemaphoreHandle_t xSPIMutex = NULL;

// Thread-safe SPI transfer
bool spi_transfer_safe(uint8_t *tx_data, uint8_t *rx_data, size_t len) {
    // Try to acquire mutex with timeout
    if (xSemaphoreTake(xSPIMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Critical section - exclusive SPI access
        HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, len, 100);
        
        // Release mutex
        xSemaphoreGive(xSPIMutex);
        return true;
    }
    
    // Timeout - SPI busy
    return false;
}

// Task A - Temperature sensor
void vTempSensorTask(void *pvParameters) {
    uint8_t cmd[] = {READ_TEMP_CMD};
    uint8_t data[2];
    
    for (;;) {
        if (spi_transfer_safe(cmd, data, sizeof(data))) {
            int16_t temp = (data[0] << 8) | data[1];
            printf("Temperature: %d\n", temp);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task B - Accelerometer
void vAccelTask(void *pvParameters) {
    uint8_t cmd[] = {READ_ACCEL_CMD};
    uint8_t data[6];
    
    for (;;) {
        if (spi_transfer_safe(cmd, data, sizeof(data))) {
            // Process accelerometer data
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 2. Shared Data Structure

```c
// Shared configuration structure
typedef struct {
    uint32_t sample_rate;
    uint32_t threshold;
    bool enabled;
} Config_t;

static Config_t g_config;
static SemaphoreHandle_t xConfigMutex;

// Thread-safe configuration update
bool config_update(uint32_t sample_rate, uint32_t threshold) {
    if (xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Atomic update of multiple fields
        g_config.sample_rate = sample_rate;
        g_config.threshold = threshold;
        
        xSemaphoreGive(xConfigMutex);
        return true;
    }
    return false;
}

// Thread-safe configuration read
Config_t config_read(void) {
    Config_t local_config;
    
    if (xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Atomic read of entire structure
        local_config = g_config;
        xSemaphoreGive(xConfigMutex);
    }
    
    return local_config;
}
```

### 3. Recursive Mutex for Nested Functions

```c
static SemaphoreHandle_t xRecursiveMutex;

void low_level_write(uint8_t *data, size_t len) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    
    // Hardware access
    HAL_UART_Transmit(&huart1, data, len, 100);
    
    xSemaphoreGiveRecursive(xRecursiveMutex);
}

void high_level_send_packet(uint8_t *payload, size_t len) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    
    // Add header
    uint8_t header[] = {START_BYTE, len};
    low_level_write(header, sizeof(header));  // Nested call OK!
    
    // Send payload
    low_level_write(payload, len);  // Another nested call
    
    // Add checksum
    uint8_t checksum = calculate_crc(payload, len);
    low_level_write(&checksum, 1);
    
    xSemaphoreGiveRecursive(xRecursiveMutex);
}
```

## Best Practices

### 1. Minimize Critical Sections

```c
// BAD - Long critical section
xSemaphoreTake(xMutex, portMAX_DELAY);
complex_calculation();      // Don't do this inside mutex!
access_shared_resource();
another_calculation();      // Don't do this either!
xSemaphoreGive(xMutex);

// GOOD - Minimal critical section
complex_calculation();      // Do calculations outside
xSemaphoreTake(xMutex, portMAX_DELAY);
access_shared_resource();  // Only protect actual access
xSemaphoreGive(xMutex);
another_calculation();      // Continue outside
```

### 2. Always Use Timeouts

```c
// BAD - Can deadlock forever
xSemaphoreTake(xMutex, portMAX_DELAY);

// GOOD - Timeout with error handling
if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Access resource
    xSemaphoreGive(xMutex);
} else {
    // Handle timeout
    log_error("Mutex timeout - possible deadlock");
}
```

### 3. Prevent Deadlocks

```c
// BAD - Potential deadlock
// Task A: Take Mutex1, then Mutex2
// Task B: Take Mutex2, then Mutex1

// GOOD - Always acquire in same order
// Both tasks: Always take Mutex1 first, then Mutex2
```

### 4. Use RAII Pattern (C++)

```cpp
class MutexGuard {
    SemaphoreHandle_t& mutex;
public:
    MutexGuard(SemaphoreHandle_t& m) : mutex(m) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
    ~MutexGuard() {
        xSemaphoreGive(mutex);
    }
};

void safe_function() {
    MutexGuard guard(xMutex);  // Automatically takes
    
    // Access shared resource
    // Mutex automatically released when guard goes out of scope
}  // Destructor gives mutex back
```

## Common Pitfalls

### 1. Priority Inversion

**Problem**: Low priority task blocks high priority task

**Solution**: Use mutex (has priority inheritance) not binary semaphore

### 2. Forgotten Give

**Problem**: Taking mutex but forgetting to give it back

```c
if (xSemaphoreTake(xMutex, timeout) == pdTRUE) {
    if (error_condition) {
        return ERROR;  // OOPS! Forgot xSemaphoreGive()
    }
    xSemaphoreGive(xMutex);
}
```

**Solution**: Always give before any return

```c
if (xSemaphoreTake(xMutex, timeout) == pdTRUE) {
    if (error_condition) {
        xSemaphoreGive(xMutex);  // Give before return
        return ERROR;
    }
    xSemaphoreGive(xMutex);
}
```

### 3. ISR Usage

**Problem**: Using mutex from ISR

**Solution**: Use binary semaphore or defer to task

### 4. Recursive Lock with Standard Mutex

**Problem**: Same task tries to take mutex twice

**Solution**: Use recursive mutex if nesting needed

## Wind Turbine Application

### Protected Resources in Wind Turbine System

1. **SPI Bus** - Multiple sensors share same SPI
   - Vibration sensor (100Hz)
   - Temperature sensors (10Hz)
   - Current sensor (50Hz)

2. **Configuration Data** - Multiple tasks update/read
   - Threshold values
   - Operating modes
   - Calibration data

3. **UART Communication** - Multiple sources send data
   - Debug messages
   - Alert notifications
   - Data logging

4. **Flash Memory** - Concurrent access for
   - Configuration storage
   - Data logging
   - Firmware updates

### Implementation Example

```c
// Wind turbine system mutexes
typedef struct {
    SemaphoreHandle_t spi_mutex;      // SPI bus protection
    SemaphoreHandle_t uart_mutex;     // UART protection
    SemaphoreHandle_t config_mutex;   // Configuration protection
    SemaphoreHandle_t flash_mutex;    // Flash memory protection
} SystemMutexes_t;

static SystemMutexes_t g_mutexes;

// Initialize all mutexes
void system_mutex_init(void) {
    g_mutexes.spi_mutex = xSemaphoreCreateMutex();
    g_mutexes.uart_mutex = xSemaphoreCreateMutex();
    g_mutexes.config_mutex = xSemaphoreCreateMutex();
    g_mutexes.flash_mutex = xSemaphoreCreateMutex();
    
    configASSERT(g_mutexes.spi_mutex != NULL);
    configASSERT(g_mutexes.uart_mutex != NULL);
    configASSERT(g_mutexes.config_mutex != NULL);
    configASSERT(g_mutexes.flash_mutex != NULL);
}

// Thread-safe vibration reading
bool read_vibration_sensor(VibrationData_t *data) {
    bool success = false;
    
    if (xSemaphoreTake(g_mutexes.spi_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Configure SPI for vibration sensor
        spi_set_speed(SPI_SPEED_HIGH);
        spi_select_device(VIBRATION_SENSOR);
        
        // Read sensor data
        success = spi_read_registers(VIBRATION_REG, 
                                     (uint8_t*)data, 
                                     sizeof(VibrationData_t));
        
        // Deselect device
        spi_deselect_device();
        
        // Release SPI bus
        xSemaphoreGive(g_mutexes.spi_mutex);
    }
    
    return success;
}
```

## Performance Considerations

### Mutex Overhead

- **Take operation**: ~100-200 CPU cycles
- **Give operation**: ~100-200 CPU cycles
- **Priority inheritance**: Additional ~50 cycles

### Optimization Tips

1. **Group Operations** - Minimize take/give cycles
2. **Use Timeouts** - Prevent infinite blocking
3. **Local Copies** - Read once, work on copy
4. **Atomic Operations** - Use for simple variables

## Testing Mutex Protection

### 1. Concurrent Access Test

```c
void test_concurrent_access(void) {
    // Create multiple tasks accessing same resource
    xTaskCreate(vTask1_SPI, "SPI1", 1024, NULL, 3, NULL);
    xTaskCreate(vTask2_SPI, "SPI2", 1024, NULL, 3, NULL);
    xTaskCreate(vTask3_SPI, "SPI3", 1024, NULL, 3, NULL);
    
    // Monitor for data corruption
    // Check SPI transaction integrity
}
```

### 2. Priority Inheritance Test

```c
void test_priority_inheritance(void) {
    // Low priority task takes mutex
    // High priority task requests mutex
    // Verify low task priority boosted
    // Verify medium task cannot preempt
}
```

### 3. Deadlock Detection

```c
void test_deadlock_prevention(void) {
    // Set short timeouts
    // Create deadlock scenario
    // Verify timeout detection
    // Verify recovery mechanism
}
```

## Summary

Mutexes are essential for protecting shared resources in multi-tasking systems. Key points:

1. **Use mutexes** for shared resource protection
2. **Priority inheritance** prevents priority inversion
3. **Minimize critical sections** for performance
4. **Always use timeouts** to prevent deadlocks
5. **Cannot use from ISRs** - use binary semaphores instead
6. **Test thoroughly** with concurrent access scenarios

The wind turbine system uses mutexes to protect SPI bus, UART, configuration data, and flash memory from corruption due to concurrent access by multiple tasks.