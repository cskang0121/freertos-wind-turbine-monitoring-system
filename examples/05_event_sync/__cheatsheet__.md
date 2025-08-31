# Event Groups for Complex Synchronization - Quick Reference

## Overview
This capability demonstrates event groups for coordinating multiple tasks based on complex conditions. Event groups use individual bits to represent different system states or events.

**Key Concepts:**
- 24 event bits per event group (on 32-bit systems)
- AND/OR logic for waiting on multiple conditions
- Broadcast mechanism (multiple tasks can wait for same bits)
- Automatic or manual bit clearing
- Synchronization barriers for coordinated startup

## Event Group Fundamentals

### Event Bits Structure
```c
// Event group holds up to 24 bits (bits 0-23)
// Each bit represents a different condition or state
#define SENSOR_READY_BIT     (1 << 0)  // 0x01
#define NETWORK_READY_BIT    (1 << 1)  // 0x02  
#define CALIBRATION_DONE_BIT (1 << 2)  // 0x04
#define ERROR_CONDITION_BIT  (1 << 3)  // 0x08

// Combination conditions
#define ALL_SYSTEMS_READY    (SENSOR_READY_BIT | NETWORK_READY_BIT | CALIBRATION_DONE_BIT)
```

### Event Group vs Other Synchronization
| Use Case | Best Choice | Why |
|----------|-------------|-----|
| Simple signaling | Binary Semaphore | Lightweight, fast |
| Data passing | Queue | Transfers actual data |
| Resource protection | Mutex | Ownership, priority inheritance |
| **Complex conditions** | **Event Group** | **Multiple bits, AND/OR logic** |
| **System state machine** | **Event Group** | **Multiple states, broadcasts** |

## Core Event Group Operations

### 1. Creation and Basic Operations
```c
// Create event group
EventGroupHandle_t xEventGroupCreate(void);

// Set bits (signal events)
EventBits_t xEventGroupSetBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToSet
);

// Clear bits
EventBits_t xEventGroupClearBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToClear
);
```

### 2. Waiting for Events
```c
// Wait for events with AND/OR logic
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToWaitFor,    // Which bits to wait for
    const BaseType_t xClearOnExit,        // Clear bits when satisfied?
    const BaseType_t xWaitForAllBits,     // AND (pdTRUE) or OR (pdFALSE)?
    TickType_t xTicksToWait               // Timeout
);
```

## Implementation Patterns

### 1. System Startup Synchronization
```c
EventGroupHandle_t xSystemReadyEvents;

// Define system readiness bits
#define SENSOR_INIT_DONE_BIT    (1 << 0)
#define NETWORK_CONNECTED_BIT   (1 << 1)
#define CONFIG_LOADED_BIT       (1 << 2)
#define ALL_SYSTEMS_READY       (SENSOR_INIT_DONE_BIT | NETWORK_CONNECTED_BIT | CONFIG_LOADED_BIT)

void vMainSystemTask(void *pvParameters) {
    // Wait for all subsystems to be ready (AND logic)
    EventBits_t result = xEventGroupWaitBits(
        xSystemReadyEvents,
        ALL_SYSTEMS_READY,     // Wait for all these bits
        pdFALSE,               // Don't clear bits
        pdTRUE,                // Wait for ALL bits (AND)
        pdMS_TO_TICKS(30000)   // 30 second timeout
    );
    
    if((result & ALL_SYSTEMS_READY) == ALL_SYSTEMS_READY) {
        // All systems ready - start main operation
        start_main_operation();
    } else {
        // Timeout or error - handle startup failure
        handle_startup_failure();
    }
}
```

### 2. Multi-Condition Monitoring
```c
// Define different types of events
#define DATA_AVAILABLE_BIT      (1 << 0)
#define NETWORK_ERROR_BIT       (1 << 1)
#define MEMORY_LOW_BIT          (1 << 2)
#define SHUTDOWN_REQUEST_BIT    (1 << 3)

void vMonitoringTask(void *pvParameters) {
    while(1) {
        // Wait for any of these conditions (OR logic)
        EventBits_t events = xEventGroupWaitBits(
            xSystemEvents,
            DATA_AVAILABLE_BIT | NETWORK_ERROR_BIT | MEMORY_LOW_BIT | SHUTDOWN_REQUEST_BIT,
            pdTRUE,    // Clear bits when satisfied
            pdFALSE,   // Wait for ANY bit (OR)
            portMAX_DELAY
        );
        
        // Handle events based on which bits were set
        if(events & DATA_AVAILABLE_BIT) {
            process_available_data();
        }
        if(events & NETWORK_ERROR_BIT) {
            handle_network_error();
        }
        if(events & MEMORY_LOW_BIT) {
            cleanup_memory();
        }
        if(events & SHUTDOWN_REQUEST_BIT) {
            perform_shutdown();
            break;
        }
    }
}
```

### 3. Synchronization Barrier
```c
// Coordinate multiple tasks to reach same point
#define TASK_A_READY_BIT    (1 << 0)
#define TASK_B_READY_BIT    (1 << 1)
#define TASK_C_READY_BIT    (1 << 2)
#define ALL_TASKS_READY     (TASK_A_READY_BIT | TASK_B_READY_BIT | TASK_C_READY_BIT)

void vTaskA(void *pvParameters) {
    // Do initialization work
    initialize_task_a();
    
    // Signal ready and wait for others
    EventBits_t result = xEventGroupSync(
        xSyncEvents,
        TASK_A_READY_BIT,     // Set this bit
        ALL_TASKS_READY,      // Wait for these bits
        pdMS_TO_TICKS(10000)  // 10 second timeout
    );
    
    if((result & ALL_TASKS_READY) == ALL_TASKS_READY) {
        // All tasks are synchronized - proceed together
        synchronized_operation();
    }
}
```

## Advanced Event Group Features

### Bit Manipulation Strategies
```c
// Check current event state without waiting
EventBits_t current_bits = xEventGroupGetBits(xEventGroup);

// Set bits from ISR context
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xEventGroupSetBitsFromISR(xEventGroup, ERROR_BIT, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

// Clear specific bits only
xEventGroupClearBits(xEventGroup, TEMPORARY_STATE_BITS);
```

### State Machine Implementation
```c
// System states as event bits
#define STATE_IDLE_BIT          (1 << 0)
#define STATE_CALIBRATING_BIT   (1 << 1)  
#define STATE_OPERATING_BIT     (1 << 2)
#define STATE_ERROR_BIT         (1 << 3)

void vStateController(void *pvParameters) {
    // Start in idle state
    xEventGroupSetBits(xStateEvents, STATE_IDLE_BIT);
    
    while(1) {
        EventBits_t current_state = xEventGroupWaitBits(
            xStateEvents,
            STATE_IDLE_BIT | STATE_CALIBRATING_BIT | STATE_OPERATING_BIT | STATE_ERROR_BIT,
            pdFALSE,  // Don't clear
            pdFALSE,  // OR logic
            portMAX_DELAY
        );
        
        if(current_state & STATE_IDLE_BIT) {
            handle_idle_state();
        } else if(current_state & STATE_CALIBRATING_BIT) {
            handle_calibration_state();
        } else if(current_state & STATE_OPERATING_BIT) {
            handle_operating_state();
        } else if(current_state & STATE_ERROR_BIT) {
            handle_error_state();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // State machine cycle time
    }
}
```

## Performance and Design Considerations

### Event Group Limitations
- **Maximum 24 bits**: Plan bit usage carefully
- **No data transfer**: Only signaling, not data passing  
- **Memory overhead**: Each event group uses ~44 bytes
- **Bit conflicts**: Multiple tasks setting/clearing same bits

### Optimization Tips
- **Group related events**: Use logical bit groupings
- **Plan bit allocation**: Document which bits represent what
- **Minimize bit clearing**: Use manual clearing when appropriate
- **Avoid busy waiting**: Don't continuously poll event bits

## Common Pitfalls

### Event Group Design Issues
- **Bit conflicts**: Multiple tasks modifying same bits
- **Infinite loops**: Waiting for bits that never get set
- **Race conditions**: Checking bits without proper synchronization
- **Bit overflow**: Using bits beyond available range (0-23)

### Logic Errors  
- **Wrong AND/OR logic**: Using pdTRUE/pdFALSE incorrectly
- **Clearing too early**: Bits cleared before all waiters satisfied
- **Timeout handling**: Not handling timeout conditions properly
- **ISR context**: Using non-ISR functions from interrupts

## Integration with Wind Turbine System

### System Startup Coordination
Our system uses event groups to coordinate complex startup:

```c
// System readiness event group
EventGroupHandle_t xSystemReadyEvents;

// Readiness bits for each major subsystem
#define SENSOR_READY_BIT       (1 << 0)
#define NETWORK_READY_BIT      (1 << 1)  
#define ANOMALY_READY_BIT      (1 << 2)
#define SAFETY_READY_BIT       (1 << 3)
#define DASHBOARD_READY_BIT    (1 << 4)

// Combined readiness states
#define MINIMAL_SYSTEM_READY   (SENSOR_READY_BIT | SAFETY_READY_BIT)
#define FULL_SYSTEM_READY      (SENSOR_READY_BIT | NETWORK_READY_BIT | ANOMALY_READY_BIT | SAFETY_READY_BIT | DASHBOARD_READY_BIT)
```

### Multi-Condition System Monitoring
- **Emergency conditions**: Multiple safety triggers (OR logic)
- **Normal operation**: All systems healthy (AND logic)  
- **Maintenance mode**: Specific subsystem combinations
- **Shutdown sequence**: Coordinated task termination

### Event Statistics
The system tracks event group usage:
- **Bits set count**: Total number of bits set
- **Wait operations**: Tasks waiting for conditions
- **Timeout events**: Failed synchronization attempts
- **Current state**: Real-time event bit status

## Best Practices
- **Document bit meanings**: Clear bit definitions and usage
- **Use meaningful names**: Self-documenting bit definitions
- **Plan bit allocation**: Avoid conflicts and overlaps
- **Handle timeouts gracefully**: Always check return values
- **Minimize complexity**: Don't over-engineer event logic
- **Test synchronization**: Verify all wait conditions work correctly