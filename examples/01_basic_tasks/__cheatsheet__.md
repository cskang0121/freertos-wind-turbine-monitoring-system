# Task Scheduling with Preemption - Quick Reference

## Overview
This capability demonstrates the foundation of FreeRTOS: creating tasks with different priorities and watching the scheduler automatically manage execution through preemptive multitasking.

**Key Concepts:**
- Priority-based preemptive scheduling
- Task Control Blocks (TCBs) and stack allocation
- System tick timer and context switching
- Task states: Ready, Running, Blocked, Suspended

## Core FreeRTOS Components

### 1. Task Creation
```c
BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,      // Function to run
    const char * const pcName,      // Task name (debugging)
    configSTACK_DEPTH_TYPE usStackDepth,  // Stack size
    void *pvParameters,             // Parameters passed to task
    UBaseType_t uxPriority,         // Task priority (0 = lowest)
    TaskHandle_t *pxCreatedTask     // Handle for later reference
);
```

### 2. Task Priorities
- **Higher number = Higher priority** (0 is lowest)
- **Preemption**: Higher priority tasks interrupt lower priority ones
- **Round-robin**: Same priority tasks share CPU time
- **Idle Task**: Priority 0, runs when nothing else can run

### 3. System Tick Timer
- **Frequency**: Typically 1000 Hz (1ms ticks)
- **Purpose**: Provides time base for scheduling decisions
- **Context Switch**: Triggered by tick interrupt + scheduler

## Implementation Flow

### System Startup Sequence
1. **main()** creates user tasks
2. **xTaskCreate()** allocates TCB + stack for each task
3. **vTaskStartScheduler()** creates Idle Task and starts tick timer
4. **Scheduler takes control** - main() never returns

### Runtime Task Switching
1. **Tick Interrupt** occurs (every 1ms)
2. **Scheduler** checks task states and priorities
3. **Context Switch** saves current task, loads next task
4. **Task Execution** continues until next interrupt

## Task States
- **Running**: Currently executing on CPU
- **Ready**: Can run but waiting for CPU
- **Blocked**: Waiting for event/resource/time
- **Suspended**: Manually paused

## Code Patterns

### Basic Task Function
```c
void vMyTask(void *pvParameters) {
    // Task initialization
    const TickType_t xDelay = pdMS_TO_TICKS(1000);  // 1 second
    
    // Task main loop
    while(1) {
        // Do task work
        printf("Task running\\n");
        
        // Yield CPU for specified time
        vTaskDelay(xDelay);
    }
}
```

### Task Creation Example
```c
// Create high priority task
xTaskCreate(vSafetyTask, "Safety", 2048, NULL, 5, &xSafetyHandle);

// Create low priority task  
xTaskCreate(vLoggerTask, "Logger", 1024, NULL, 1, &xLoggerHandle);
```

## Common Pitfalls
- **Stack Overflow**: Underestimating stack size requirements
- **Priority Inversion**: Not designing priority hierarchy properly
- **Starvation**: Low priority tasks never getting CPU time
- **Blocking in ISR**: Never call blocking functions in interrupt handlers

## Integration with Wind Turbine System
In our system, task priorities are carefully designed:
- **Safety Task (Priority 6)**: Highest - emergency response
- **Sensor Task (Priority 4)**: High - real-time data acquisition  
- **Anomaly Task (Priority 3)**: Medium - data processing
- **Network Task (Priority 2)**: Low - cloud communication
- **Dashboard Task (Priority 1)**: Lowest - UI updates

This hierarchy ensures critical functions always take precedence over less critical ones.

## Best Practices
- **Start with generous stack sizes**, optimize later
- **Use meaningful task names** for debugging
- **Design clear priority hierarchy** based on criticality
- **Avoid infinite loops without delays** - always yield CPU
- **Consider task interdependencies** when setting priorities