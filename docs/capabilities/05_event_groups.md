# Capability 5: Event Groups for Complex Synchronization

## Overview

Event Groups provide a powerful mechanism for synchronizing tasks based on multiple conditions. Unlike binary semaphores (single event) or queues (data transfer), event groups allow tasks to wait for combinations of events using bitwise flags. This is ideal for complex state machines and multi-condition synchronization.

## Core Concepts

### 1. Event Bits and Flags

An event group is a set of binary flags (bits), where each bit represents a specific event:

```
Event Group (24 bits available on 32-bit systems):
Bit 0: WIFI_CONNECTED
Bit 1: SENSOR_READY  
Bit 2: ANOMALY_DETECTED
Bit 3: MAINTENANCE_MODE
...
Bit 23: SYSTEM_ERROR

Current State: 0x00000007 = 0000 0000 0000 0000 0000 0111
                                                       |||
                                                       ||└─ WIFI_CONNECTED (1)
                                                       |└── SENSOR_READY (1)
                                                       └─── ANOMALY_DETECTED (1)
```

### 2. Waiting for Events

Tasks can wait for events using AND or OR logic:

```c
// AND Logic - Wait for ALL specified bits
// Task blocks until BOTH wifi AND sensor are ready
EventBits_t bits = xEventGroupWaitBits(
    xEventGroup,
    WIFI_BIT | SENSOR_BIT,  // Bits to wait for
    pdTRUE,                  // Clear bits after
    pdTRUE,                  // Wait for ALL (AND)
    portMAX_DELAY
);

// OR Logic - Wait for ANY specified bit
// Task proceeds when EITHER anomaly OR emergency occurs
EventBits_t bits = xEventGroupWaitBits(
    xEventGroup,
    ANOMALY_BIT | EMERGENCY_BIT,
    pdFALSE,                 // Don't clear
    pdFALSE,                 // Wait for ANY (OR)
    timeout
);
```

### 3. Setting and Clearing Events

```c
// Set events - can wake multiple waiting tasks
xEventGroupSetBits(xEventGroup, WIFI_BIT | SENSOR_BIT);

// Clear events
xEventGroupClearBits(xEventGroup, MAINTENANCE_BIT);

// Set from ISR (deferred to timer task)
xEventGroupSetBitsFromISR(xEventGroup, INTERRUPT_BIT, &xHigherPriorityTaskWoken);
```

## FreeRTOS Event Group APIs

### Creating an Event Group

```c
// Dynamic allocation
EventGroupHandle_t xEventGroup = xEventGroupCreate();

// Static allocation
StaticEventGroup_t xEventGroupBuffer;
EventGroupHandle_t xEventGroup = xEventGroupCreateStatic(&xEventGroupBuffer);
```

### Waiting for Events

```c
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t xEventGroup,  // Event group handle
    const EventBits_t uxBitsToWaitFor,  // Bits to wait for
    const BaseType_t xClearOnExit,      // Clear bits when unblocked?
    const BaseType_t xWaitForAllBits,   // AND (pdTRUE) or OR (pdFALSE)
    TickType_t xTicksToWait            // Timeout
);

// Example: Wait for system ready (multiple conditions)
#define WIFI_CONNECTED_BIT    (1 << 0)
#define SENSORS_READY_BIT     (1 << 1)
#define CONFIG_LOADED_BIT     (1 << 2)
#define SYSTEM_READY_BITS     (WIFI_CONNECTED_BIT | SENSORS_READY_BIT | CONFIG_LOADED_BIT)

EventBits_t uxBits = xEventGroupWaitBits(
    xSystemEventGroup,
    SYSTEM_READY_BITS,
    pdFALSE,  // Don't clear (other tasks may need to know)
    pdTRUE,   // Wait for ALL bits
    pdMS_TO_TICKS(5000)
);

if ((uxBits & SYSTEM_READY_BITS) == SYSTEM_READY_BITS) {
    // All conditions met - system ready
} else {
    // Timeout - check which bits are missing
    if (!(uxBits & WIFI_CONNECTED_BIT)) {
        // WiFi not connected
    }
}
```

### Setting Events

```c
// Set bits - wakes waiting tasks
EventBits_t xEventGroupSetBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToSet
);

// Set bits from ISR (deferred)
BaseType_t xEventGroupSetBitsFromISR(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToSet,
    BaseType_t *pxHigherPriorityTaskWoken
);

// Example: Signal multiple events
void sensor_init_complete() {
    xEventGroupSetBits(xSystemEventGroup, 
                       SENSORS_READY_BIT | CALIBRATION_DONE_BIT);
}
```

### Clearing Events

```c
// Clear bits
EventBits_t xEventGroupClearBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToClear
);

// Clear from ISR
EventBits_t xEventGroupClearBitsFromISR(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToClear
);
```

### Synchronization Point

```c
// Synchronize multiple tasks at a barrier
EventBits_t xEventGroupSync(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToSet,      // This task's contribution
    const EventBits_t uxBitsToWaitFor,  // Wait for all tasks
    TickType_t xTicksToWait
);

// Example: Three tasks must reach sync point
#define TASK_A_SYNC_BIT (1 << 0)
#define TASK_B_SYNC_BIT (1 << 1)
#define TASK_C_SYNC_BIT (1 << 2)
#define ALL_SYNC_BITS   (TASK_A_SYNC_BIT | TASK_B_SYNC_BIT | TASK_C_SYNC_BIT)

// In Task A:
xEventGroupSync(xSyncEventGroup, TASK_A_SYNC_BIT, ALL_SYNC_BITS, portMAX_DELAY);
// Blocks until Tasks B and C also reach their sync points
```

## Implementation Patterns

### 1. System State Machine

```c
// Define states as bit combinations
#define STATE_IDLE           0x00
#define STATE_INITIALIZING   (INIT_BIT)
#define STATE_OPERATIONAL    (INIT_BIT | READY_BIT)
#define STATE_MAINTENANCE    (INIT_BIT | READY_BIT | MAINT_BIT)
#define STATE_ERROR          (ERROR_BIT)

// State transition functions
void enter_operational_mode() {
    xEventGroupClearBits(xStateGroup, ALL_STATE_BITS);
    xEventGroupSetBits(xStateGroup, STATE_OPERATIONAL);
}

// Tasks wait for specific states
void operational_task() {
    while (1) {
        EventBits_t state = xEventGroupWaitBits(
            xStateGroup,
            STATE_OPERATIONAL,
            pdFALSE,  // Don't clear
            pdTRUE,   // Exact match
            portMAX_DELAY
        );
        
        // Do operational work
        perform_operations();
    }
}
```

### 2. Multi-Condition Data Transmission

```c
// Network task waits for multiple conditions before sending
#define DATA_READY_BIT       (1 << 0)
#define NETWORK_READY_BIT    (1 << 1)
#define ANOMALY_DETECTED_BIT (1 << 2)
#define AUTH_COMPLETE_BIT    (1 << 3)

void network_task() {
    while (1) {
        // Wait for: data ready AND network ready AND authenticated
        // AND (anomaly detected OR scheduled transmission)
        EventBits_t bits = xEventGroupWaitBits(
            xEventGroup,
            DATA_READY_BIT | NETWORK_READY_BIT | AUTH_COMPLETE_BIT,
            pdTRUE,   // Clear after transmission
            pdTRUE,   // Need ALL conditions
            portMAX_DELAY
        );
        
        // Check if anomaly triggered transmission
        if (bits & ANOMALY_DETECTED_BIT) {
            send_priority_alert();
        } else {
            send_regular_data();
        }
    }
}
```

### 3. Startup Sequence Coordination

```c
// Coordinate system startup with dependencies
#define RTOS_STARTED_BIT     (1 << 0)
#define DRIVERS_LOADED_BIT   (1 << 1)
#define SENSORS_INIT_BIT     (1 << 2)
#define NETWORK_INIT_BIT     (1 << 3)
#define CONFIG_LOADED_BIT    (1 << 4)
#define SYSTEM_READY_BIT     (1 << 5)

void startup_coordinator() {
    // Wait for RTOS
    xEventGroupWaitBits(xStartupEvents, RTOS_STARTED_BIT, 
                       pdFALSE, pdTRUE, portMAX_DELAY);
    
    // Initialize drivers
    init_drivers();
    xEventGroupSetBits(xStartupEvents, DRIVERS_LOADED_BIT);
    
    // Wait for drivers before initializing sensors
    xEventGroupWaitBits(xStartupEvents, DRIVERS_LOADED_BIT,
                       pdFALSE, pdTRUE, portMAX_DELAY);
    
    // Initialize sensors and network in parallel
    init_sensors();
    xEventGroupSetBits(xStartupEvents, SENSORS_INIT_BIT);
    
    // Wait for all subsystems
    EventBits_t ready = xEventGroupWaitBits(
        xStartupEvents,
        SENSORS_INIT_BIT | NETWORK_INIT_BIT | CONFIG_LOADED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(10000)
    );
    
    if (ready == (SENSORS_INIT_BIT | NETWORK_INIT_BIT | CONFIG_LOADED_BIT)) {
        xEventGroupSetBits(xStartupEvents, SYSTEM_READY_BIT);
        printf("System ready for operation\n");
    }
}
```

### 4. Emergency Response

```c
#define VIBRATION_ALARM_BIT  (1 << 0)
#define TEMP_ALARM_BIT       (1 << 1)
#define CURRENT_ALARM_BIT    (1 << 2)
#define EMERGENCY_STOP_BIT   (1 << 7)
#define ANY_ALARM_BITS       (VIBRATION_ALARM_BIT | TEMP_ALARM_BIT | CURRENT_ALARM_BIT)

void safety_monitor_task() {
    while (1) {
        // Wait for ANY alarm condition
        EventBits_t alarms = xEventGroupWaitBits(
            xAlarmEvents,
            ANY_ALARM_BITS,
            pdFALSE,  // Don't auto-clear (need to log first)
            pdFALSE,  // OR logic - any alarm triggers
            portMAX_DELAY
        );
        
        // Log which alarms triggered
        if (alarms & VIBRATION_ALARM_BIT) log_alarm("Vibration");
        if (alarms & TEMP_ALARM_BIT) log_alarm("Temperature");
        if (alarms & CURRENT_ALARM_BIT) log_alarm("Current");
        
        // Critical: Set emergency stop
        xEventGroupSetBits(xAlarmEvents, EMERGENCY_STOP_BIT);
        
        // Clear alarm bits after handling
        xEventGroupClearBits(xAlarmEvents, ANY_ALARM_BITS);
        
        // Notify all tasks of emergency
        broadcast_emergency();
    }
}
```

## Best Practices

### 1. Bit Definition Organization

```c
// Group related bits together
// System State Bits (0-7)
#define STATE_INIT_BIT       (1 << 0)
#define STATE_READY_BIT      (1 << 1)
#define STATE_RUNNING_BIT    (1 << 2)
#define STATE_ERROR_BIT      (1 << 3)

// Alarm Bits (8-15)
#define ALARM_VIBRATION_BIT  (1 << 8)
#define ALARM_TEMP_BIT       (1 << 9)
#define ALARM_CURRENT_BIT    (1 << 10)

// Control Bits (16-23)
#define CTRL_STOP_BIT        (1 << 16)
#define CTRL_MAINTENANCE_BIT (1 << 17)
```

### 2. Clear-on-Exit Strategy

```c
// Auto-clear for one-time events
xEventGroupWaitBits(xEventGroup, DATA_READY_BIT,
                    pdTRUE,  // Clear automatically
                    pdTRUE, pdTRUE, timeout);

// Don't clear for persistent states
xEventGroupWaitBits(xEventGroup, SYSTEM_READY_BIT,
                    pdFALSE,  // State remains set
                    pdTRUE, pdTRUE, timeout);
```

### 3. Timeout Handling

```c
EventBits_t bits = xEventGroupWaitBits(
    xEventGroup, REQUIRED_BITS,
    pdFALSE, pdTRUE, pdMS_TO_TICKS(5000)
);

// Check if all bits were set or timeout occurred
if ((bits & REQUIRED_BITS) == REQUIRED_BITS) {
    // Success - all conditions met
} else {
    // Timeout - determine what's missing
    EventBits_t current = xEventGroupGetBits(xEventGroup);
    EventBits_t missing = REQUIRED_BITS & ~current;
    handle_missing_events(missing);
}
```

## Common Pitfalls

### 1. Bit Limit (24 bits max)

```c
// WRONG - Using bit 24+ (undefined behavior)
#define INVALID_BIT (1 << 24)  // Bad! Only 0-23 available

// CORRECT - Use multiple event groups if needed
EventGroupHandle_t xEventGroup1;  // Bits 0-23
EventGroupHandle_t xEventGroup2;  // Additional bits
```

### 2. Race Conditions with Clear

```c
// PROBLEM - Race condition
EventBits_t bits = xEventGroupGetBits(xEventGroup);
if (bits & MY_BIT) {
    // Another task might clear between check and clear!
    xEventGroupClearBits(xEventGroup, MY_BIT);
}

// SOLUTION - Atomic wait and clear
EventBits_t bits = xEventGroupWaitBits(
    xEventGroup, MY_BIT,
    pdTRUE,  // Atomic clear
    pdTRUE, pdTRUE, 0
);
```

### 3. ISR Limitations

```c
// WRONG - Cannot wait in ISR
void My_ISR() {
    xEventGroupWaitBits(...);  // Will crash!
}

// CORRECT - Only set/clear from ISR
void My_ISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(xEventGroup, ISR_BIT, 
                              &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Wind Turbine Application

### Event Architecture for Wind Turbine

```c
// System Events
#define TURBINE_STARTED_BIT     (1 << 0)
#define SENSORS_ONLINE_BIT      (1 << 1)
#define NETWORK_CONNECTED_BIT   (1 << 2)
#define CLOUD_AUTHENTICATED_BIT (1 << 3)

// Operational Events  
#define ANOMALY_DETECTED_BIT    (1 << 8)
#define MAINTENANCE_DUE_BIT     (1 << 9)
#define DATA_BUFFER_FULL_BIT    (1 << 10)

// Safety Events
#define OVERSPEED_BIT          (1 << 16)
#define VIBRATION_LIMIT_BIT    (1 << 17)
#define TEMP_LIMIT_BIT         (1 << 18)
#define EMERGENCY_STOP_BIT     (1 << 23)

// Operational task waits for system ready
void turbine_operation_task() {
    // Wait for complete system initialization
    EventBits_t ready = xEventGroupWaitBits(
        xSystemEvents,
        TURBINE_STARTED_BIT | SENSORS_ONLINE_BIT | NETWORK_CONNECTED_BIT,
        pdFALSE, pdTRUE, portMAX_DELAY
    );
    
    while (1) {
        // Monitor for anomalies or maintenance
        EventBits_t events = xEventGroupWaitBits(
            xOperationalEvents,
            ANOMALY_DETECTED_BIT | MAINTENANCE_DUE_BIT,
            pdTRUE,   // Clear after handling
            pdFALSE,  // OR - either event
            pdMS_TO_TICKS(1000)
        );
        
        if (events & ANOMALY_DETECTED_BIT) {
            trigger_predictive_maintenance();
        }
        if (events & MAINTENANCE_DUE_BIT) {
            enter_maintenance_mode();
        }
    }
}
```

## Performance Considerations

### Memory Usage
- Each event group: ~80 bytes
- No per-bit storage overhead
- Efficient for multiple conditions

### CPU Overhead
- Set/Clear: ~50-100 cycles
- Wait operation: ~100-200 cycles
- Multiple task wake: ~200 cycles per task

### Advantages over Alternatives

| Feature | Event Groups | Binary Semaphores | Queues |
|---------|--------------|-------------------|---------|
| Multiple conditions | ✓ Excellent | ✗ One at a time | ✗ Not designed for |
| Broadcast to tasks | ✓ Built-in | ✗ Single task | ✗ Single reader |
| Memory efficiency | ✓ 24 events/group | ✗ 1 event/semaphore | ✗ Data storage |
| Complex logic | ✓ AND/OR | ✗ Simple | ✗ Not applicable |

## Testing Event Groups

### 1. Multi-Condition Test

```c
void test_multi_condition() {
    // Set bits individually
    xEventGroupSetBits(xEventGroup, BIT_1);
    vTaskDelay(100);
    xEventGroupSetBits(xEventGroup, BIT_2);
    vTaskDelay(100);
    xEventGroupSetBits(xEventGroup, BIT_3);
    
    // Waiting task should unblock only after all 3 bits set
}
```

### 2. Broadcast Test

```c
void test_broadcast() {
    // Create 3 tasks waiting for same event
    // Set event once
    // Verify all 3 tasks wake simultaneously
}
```

### 3. Timeout Test

```c
void test_timeout() {
    EventBits_t bits = xEventGroupWaitBits(
        xEventGroup, NEVER_SET_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(1000)
    );
    
    configASSERT((bits & NEVER_SET_BIT) == 0);  // Should timeout
}
```

## Summary

Event Groups excel at:
1. **Multi-condition synchronization** - Wait for complex combinations
2. **Broadcasting events** - Wake multiple tasks simultaneously
3. **State machines** - Represent system states as bit patterns
4. **Event-driven architecture** - Loosely coupled task coordination
5. **Startup sequences** - Coordinate dependent initializations
6. **Emergency handling** - Multiple triggers for safety response

Use event groups when you need to synchronize based on multiple conditions that would be complex to implement with semaphores or queues alone.