# Capability 1: Task Scheduling with Preemption

## Overview

Task scheduling is the heart of any RTOS. FreeRTOS uses priority-based preemptive scheduling to ensure that the most important tasks get CPU time when they need it. This is crucial for real-time systems like our wind turbine monitor where safety-critical tasks must respond immediately.

## Learning Objectives

After completing this capability, you will:
1. Understand how FreeRTOS schedules tasks
2. Create tasks with appropriate priorities
3. Size task stacks correctly
4. Implement preemptive multitasking
5. Debug task-related issues

## Theory

### What is a Task?

A task is an independent thread of execution that has:
- Its own stack space
- A priority level
- A current state (Running, Ready, Blocked, Suspended)
- Optional task-local storage

### Task States

```
    Created
       |
       v
    Ready <-----> Running
       ^            |
       |            v
    Blocked    Suspended
```

- **Running**: Currently executing on the CPU
- **Ready**: Ready to run but waiting for CPU
- **Blocked**: Waiting for an event (delay, semaphore, queue)
- **Suspended**: Manually suspended, won't run until resumed

### Priority-Based Scheduling

FreeRTOS assigns each task a priority from 0 (lowest) to configMAX_PRIORITIES-1 (highest).

Key rules:
1. Higher priority tasks always preempt lower priority tasks
2. Tasks of equal priority share CPU time (round-robin)
3. A task runs until it blocks, yields, or is preempted

### Preemption

Preemption occurs when:
- A higher priority task becomes ready (unblocks)
- An interrupt makes a higher priority task ready
- The running task blocks or delays

## Implementation

### Creating a Task

```c
BaseType_t xTaskCreate(
    TaskFunction_t pxTaskCode,      /* Function that implements the task */
    const char * const pcName,      /* Text name for debugging */
    const uint16_t usStackDepth,    /* Stack size in words (not bytes!) */
    void * const pvParameters,      /* Parameter passed to task */
    UBaseType_t uxPriority,         /* Task priority */
    TaskHandle_t * const pxCreatedTask  /* Handle to created task */
);
```

### Example: Safety Task (Highest Priority)

```c
/* Task handle */
TaskHandle_t xSafetyTaskHandle = NULL;

/* Task function */
void vSafetyTask(void *pvParameters)
{
    const TickType_t xCheckPeriod = pdMS_TO_TICKS(100);  /* 100ms */
    
    for (;;) {
        /* Critical safety checks */
        if (checkEmergencyStop()) {
            triggerEmergencyShutdown();
        }
        
        if (getBladeRPM() > MAX_SAFE_RPM) {
            activateBrakes();
        }
        
        /* Must delay to allow lower priority tasks to run */
        vTaskDelay(xCheckPeriod);
    }
}

/* Create the task */
xTaskCreate(
    vSafetyTask,                    /* Function */
    "Safety",                       /* Name */
    configMINIMAL_STACK_SIZE * 2,   /* Stack: 512 words = 2KB */
    NULL,                           /* No parameters */
    configMAX_PRIORITIES - 1,       /* Highest priority */
    &xSafetyTaskHandle              /* Store handle */
);
```

## Stack Sizing

### How to Determine Stack Size

1. **Start Conservative**: Begin with 2-4x configMINIMAL_STACK_SIZE
2. **Measure Usage**: Use uxTaskGetStackHighWaterMark()
3. **Optimize**: Reduce to usage + 25% safety margin

### Stack Size Guidelines

| Task Type | Typical Stack Size | Reason |
|-----------|-------------------|---------|
| Simple GPIO | 128-256 words | Minimal local variables |
| Sensor Reading | 256-512 words | Moderate processing |
| Network | 1024-2048 words | Large buffers |
| AI Inference | 2048-4096 words | Complex algorithms |

### Checking Stack Usage

```c
void vCheckStackUsage(TaskHandle_t xTask)
{
    UBaseType_t uxHighWaterMark;
    
    /* Get minimum free stack space */
    uxHighWaterMark = uxTaskGetStackHighWaterMark(xTask);
    
    printf("Task has %lu words unused stack\n", 
           (unsigned long)uxHighWaterMark);
    
    if (uxHighWaterMark < 50) {
        printf("WARNING: Stack nearly full!\n");
    }
}
```

## Priority Design

### Wind Turbine System Priorities

```c
/* Priority assignments for our system */
#define SAFETY_TASK_PRIORITY      7  /* Emergency stop, overspeed */
#define SENSOR_TASK_PRIORITY      6  /* Real-time data acquisition */
#define ANOMALY_TASK_PRIORITY     5  /* AI inference */
#define CONTROL_TASK_PRIORITY     4  /* Control algorithms */
#define NETWORK_TASK_PRIORITY     2  /* Cloud communication */
#define LOGGING_TASK_PRIORITY     1  /* SD card logging */
/* Idle task is priority 0 */
```

### Priority Guidelines

1. **Safety Critical**: Highest priority (must never be delayed)
2. **Real-Time Data**: High priority (time-sensitive)
3. **Processing**: Medium priority (important but not urgent)
4. **Communication**: Low priority (can tolerate delays)
5. **Maintenance**: Lowest priority (background tasks)

## Common Patterns

### Pattern 1: Periodic Task

```c
void vPeriodicTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000);  /* 1 second */
    
    for (;;) {
        /* Do periodic work */
        doWork();
        
        /* Wait until next period */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
```

### Pattern 2: Event-Driven Task

```c
void vEventTask(void *pvParameters)
{
    uint32_t ulNotificationValue;
    
    for (;;) {
        /* Wait for notification (blocks indefinitely) */
        if (xTaskNotifyWait(0, 0, &ulNotificationValue, portMAX_DELAY)) {
            /* Process event */
            handleEvent(ulNotificationValue);
        }
    }
}
```

### Pattern 3: Continuous Processing

```c
void vProcessingTask(void *pvParameters)
{
    for (;;) {
        /* Process data */
        processNextDataItem();
        
        /* Yield to same/higher priority tasks */
        taskYIELD();
    }
}
```

## Debugging

### Using vTaskList()

```c
void vPrintTaskList(void)
{
    char pcBuffer[512];
    
    vTaskList(pcBuffer);
    printf("\nTask          State   Priority  Stack    #\n");
    printf("******************************************\n");
    printf("%s\n", pcBuffer);
}
```

Output:
```
Task          State   Priority  Stack    #
******************************************
Safety        R       7         450      1
Sensor        B       6         380      2
Network       B       2         812      3
IDLE          R       0         95       4
```

### Common Issues and Solutions

| Problem | Cause | Solution |
|---------|-------|----------|
| Task doesn't run | Priority too low | Increase priority or add delays to higher tasks |
| Stack overflow | Stack too small | Increase stack size |
| System hangs | Task not yielding | Add vTaskDelay() or taskYIELD() |
| Erratic behavior | Priority inversion | Use mutexes with priority inheritance |

## Exercises

### Exercise 1: Create Three Tasks
Create three tasks with different priorities and observe preemption:
- High: Blinks LED every 500ms
- Medium: Prints sensor data every 1s
- Low: Calculates statistics every 2s

### Exercise 2: Stack Optimization
1. Create a task with 4KB stack
2. Measure actual usage with uxTaskGetStackHighWaterMark()
3. Reduce stack to optimal size
4. Verify no overflow occurs

### Exercise 3: Priority Experiment
1. Create two tasks with same priority
2. Observe round-robin scheduling
3. Change one priority and observe preemption
4. Document the behavior differences

## Real-World Application

In our wind turbine system, proper task scheduling ensures:
- **Safety**: Emergency stop always responds within 10ms
- **Reliability**: Sensor data never lost due to priority
- **Efficiency**: CPU time allocated based on importance
- **Maintainability**: Clear priority scheme for debugging

## Key Takeaways

1. **Always use vTaskDelay()**: Tasks must yield CPU time
2. **Size stacks carefully**: Too small = crash, too large = waste RAM
3. **Design priorities thoughtfully**: Safety > Data > Processing > Communication
4. **Monitor stack usage**: Use high water mark in development
5. **Test preemption**: Verify higher priority tasks interrupt lower ones

## Next Steps

Now that you understand task scheduling:
1. Implement the three main tasks in the project
2. Add stack monitoring to your tasks
3. Move on to Capability 2: Interrupt Handling

## Additional Resources

- [FreeRTOS Task Documentation](https://www.freertos.org/a00019.html)
- [Task States Explained](https://www.freertos.org/RTOS-task-states.html)
- [Stack Overflow Detection](https://www.freertos.org/Stacks-and-stack-overflow-checking.html)

---

## 🎉 Integration Complete - Wind Turbine Monitor

### Full System Implementation

We've successfully integrated all concepts into a complete Wind Turbine Monitoring System:

**Location**: `src/integrated/`

**Features Implemented**:
- ✅ 5 concurrent tasks with priorities 1-6
- ✅ Real-time sensor monitoring (vibration, temperature, RPM)
- ✅ Safety monitoring with emergency stop
- ✅ Anomaly detection with statistical analysis
- ✅ Network communication simulation
- ✅ Live dashboard with task visualization
- ✅ Preemption demonstration with event logging

### Running the Integrated System

```bash
cd build/simulation
./src/integrated/turbine_monitor
```

### Lessons Learned from Integration

#### 1. POSIX Simulation Limitations
- **Challenge**: Runtime statistics return 0 in POSIX port
- **Solution**: Implemented frequency-based CPU estimation
- **Learning**: Simulation differs from real hardware

#### 2. Observer Problem
- **Challenge**: Dashboard only sees system when it's running
- **Solution**: Added preemption event history
- **Learning**: Observation affects what you can measure

#### 3. Stack Usage in Simulation
- **Challenge**: Minimal stack usage in host OS environment
- **Solution**: Added minimum values for visibility
- **Learning**: Host OS memory != embedded memory

#### 4. Context Switch Tracking
- **Challenge**: No real context switch counter
- **Solution**: Estimated based on task frequencies
- **Learning**: Some metrics need hardware support

### POSIX Port Specifics

When running on Mac/Linux, be aware:
- Tasks run as pthreads (not lightweight embedded tasks)
- No hardware performance counters
- Signals used instead of real interrupts
- Memory managed by host OS

Despite limitations, the simulation accurately demonstrates:
- Priority-based preemption
- Task state transitions
- Real-time scheduling behavior

### Documentation

- 📊 [Dashboard Guide](../../docs/DASHBOARD_GUIDE.md) - Understanding the live dashboard
- 🔧 [Integrated System README](../../src/integrated/README.md) - Full system details
- ✅ [Learning Progress](../../LEARNING_PROGRESS.md) - Track your journey

### What Makes This Integration Special

1. **Real-World Application**: Not just a demo, but a practical turbine monitor
2. **Multiple Priorities**: Shows complex interaction between 5 tasks
3. **Visual Feedback**: Live dashboard shows scheduling in action
4. **Error Handling**: Includes safety systems and anomaly detection
5. **Documentation**: Comprehensive guides for learning

### Next Capabilities to Integrate

The system is ready for:
- **Capability 2**: Add real ISR handling for sensor interrupts
- **Capability 3**: Replace global state with queues
- **Capability 4**: Add mutex protection for shared resources
- **Capability 5**: Use event groups for task synchronization

---

*Capability 1 Status: FULLY INTEGRATED ✅*
*Last Updated: 2025-08-31*