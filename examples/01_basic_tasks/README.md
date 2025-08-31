# Example 01: Basic Tasks and Priority Scheduling

## Learning Objectives

This example teaches you:
1. How to create FreeRTOS tasks using `xTaskCreate()`
2. Understanding priority-based preemptive scheduling
3. How higher priority tasks interrupt lower priority ones
4. Using `vTaskDelay()` and `vTaskDelayUntil()` for timing
5. Monitoring task states and system status

## Key Concepts

### Task Priorities
- **Idle Task**: Priority 0 (lowest)
- **Low Priority**: Priority 1 (background work)
- **Medium Priority**: Priority 2 (sensor processing)
- **High Priority**: Priority 3 (safety critical)

### Preemption
When a higher priority task becomes ready to run, it immediately preempts (interrupts) any lower priority task that is currently running.

## What You'll See

When you run this example, you'll observe:

1. **HIGH priority task** (safety checks) runs every 1.5 seconds and interrupts everything
2. **MEDIUM priority task** (sensor processing) runs every 2 seconds and interrupts LOW
3. **LOW priority task** (background work) runs every 3 seconds when nothing else needs CPU
4. **Monitor task** reports system status every 10 seconds

## Expected Output

```
[HIGH] CRITICAL CHECK #1 - Counter: 1
*** [HIGH] Checking blade RPM... OK ***
*** [HIGH] Checking emergency stop... OK ***

[MEDIUM] Executing - Counter: 2 - Processing sensors...
[MEDIUM] Reading temperature: 25.5C
[MEDIUM] Reading vibration: 0.02g

[LOW] Executing - Counter: 3 - Background work...
[LOW] Work completed, sleeping for 3 seconds
```

## Integration with Final System

### How This Capability Is Used
In the complete wind turbine monitoring system, task scheduling forms the foundation:
- **SafetyTask (Priority 5)**: Highest priority for emergency response
- **SensorTask (Priority 4)**: Regular sensor readings
- **AnomalyTask (Priority 3)**: Detection processing
- **NetworkTask (Priority 2)**: Cloud communication
- **LoggerTask (Priority 1)**: Background logging

### Console Dashboard Preview
In the integrated system, you'll see real-time task monitoring:
```
TASK MONITORING:
  Task         Priority  CPU%   State      Last Run
  Safety       5         2%     Blocked    <1ms ago
  Sensor       4         15%    Running    Now
  Anomaly      3         8%     Ready      5ms ago
  Network      2         3%     Blocked    100ms ago
  Logger       1         2%     Ready      500ms ago

PREEMPTION EVENTS:
  [10:23:45.123] Safety preempted Sensor (emergency check)
  [10:23:45.456] Sensor resumed after Safety completed
  [10:23:45.789] Anomaly preempted Logger (data ready)
```

## Experiments to Try

### Experiment 1: Change Priorities
Edit the priority definitions and observe how task execution order changes:
```c
#define LOW_PRIORITY_TASK     (tskIDLE_PRIORITY + 3)  /* Now highest! */
#define HIGH_PRIORITY_TASK    (tskIDLE_PRIORITY + 1)  /* Now lowest! */
```

### Experiment 2: Remove Delays
Comment out one of the `vTaskDelay()` calls and see what happens:
- Task will "hog" the CPU
- Lower priority tasks may never run (starvation)

### Experiment 3: Add Your Own Task
Create a new task with priority 2.5 (between MEDIUM and HIGH):
```c
void vMyCustomTask(void *pvParameters) {
    for(;;) {
        printf("[CUSTOM] My task is running!\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Experiment 4: Stack Size
Reduce stack size and observe stack overflow:
```c
configMINIMAL_STACK_SIZE * 1  /* May cause overflow */
```

## How to Build and Run

```bash
# From the project root directory
cd examples/01_basic_tasks

# For simulation (Mac/Linux)
gcc -o basic_tasks main.c -I../../external/FreeRTOS-Kernel/include \
    -I../../external/FreeRTOS-Kernel/portable/ThirdParty/GCC/Posix \
    -I../../config -pthread

./basic_tasks
```

## Connection to Wind Turbine System

In our real wind turbine predictive maintenance system:
- **High Priority**: Emergency stop, overspeed protection
- **Medium Priority**: Sensor data collection, anomaly detection
- **Low Priority**: Data logging, statistics, cloud sync

## Key Takeaways

1. **Always use delays**: Tasks must yield CPU time
2. **Priority matters**: Critical tasks get higher priority
3. **Stack size is important**: Too small causes overflow
4. **Preemption is automatic**: FreeRTOS handles task switching
5. **Design for concurrency**: Tasks run "simultaneously"

## Next Steps

Once you understand this example:
1. Move to Example 02: ISR and Deferred Processing
2. Implement these concepts in the main project
3. Add your own experimental task

## Troubleshooting

**Problem**: Tasks don't seem to preempt
- Check priorities are different
- Ensure higher priority task becomes ready (unblocks)

**Problem**: Stack overflow
- Increase stack size
- Reduce local variables
- Check for recursion

**Problem**: One task stops others from running
- Add vTaskDelay() to yield CPU
- Check for infinite loops without delays