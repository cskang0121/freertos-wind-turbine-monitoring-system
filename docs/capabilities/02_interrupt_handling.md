# Capability 2: Interrupt Service Routines with Deferred Processing

## Overview

Interrupt Service Routines (ISRs) are critical for real-time responsiveness in embedded systems. In our wind turbine predictive maintenance system, ISRs handle time-critical events like vibration spikes, emergency stops, and periodic sensor sampling.

## Key Concepts

### 1. ISR Constraints

ISRs run in a special context with significant constraints:
- **No blocking calls** - Cannot use `vTaskDelay()`, `xQueueReceive()` without timeout
- **Minimal execution time** - Target < 100 instructions
- **Special API variants** - Must use `FromISR` versions of FreeRTOS APIs
- **Stack limitations** - ISRs use system stack, not task stack

### 2. Deferred Processing Pattern

```
ISR Context                    Task Context
    |                              |
    v                              v
[Interrupt] --> [Minimal ISR] --> [Semaphore] --> [Deferred Task] --> [Heavy Processing]
                     |                                    |
                     |                                    v
                < 100 instructions              Unlimited processing time
```

### 3. FreeRTOS ISR APIs

| Regular API | ISR API | Purpose |
|------------|---------|---------|
| `xSemaphoreGive()` | `xSemaphoreGiveFromISR()` | Signal from ISR |
| `xQueueSend()` | `xQueueSendFromISR()` | Send data from ISR |
| `xEventGroupSetBits()` | `xEventGroupSetBitsFromISR()` | Set event from ISR |

### 4. Context Switching from ISR

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

// Give semaphore from ISR
xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);

// Request context switch if needed
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

## Wind Turbine Application

### Critical Interrupts

1. **Vibration Sensor (100Hz)**
   - Detect anomalous vibrations
   - Trigger immediate analysis
   - Latency requirement: < 1ms

2. **Emergency Stop Button**
   - Highest priority interrupt
   - Immediate turbine shutdown
   - Latency requirement: < 100μs

3. **RPM Sensor**
   - Monitor blade rotation
   - Detect overspeed conditions
   - Latency requirement: < 5ms

## Implementation Strategy

### Step 1: Minimal ISR

```c
void vVibrationISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // 1. Clear interrupt flag (hardware specific)
    CLEAR_INTERRUPT_FLAG();
    
    // 2. Read sensor value (minimal processing)
    uint32_t sensorValue = READ_SENSOR_REGISTER();
    
    // 3. Send to deferred task via queue
    xQueueSendFromISR(xSensorQueue, &sensorValue, &xHigherPriorityTaskWoken);
    
    // 4. Yield if high priority task woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Step 2: Deferred Processing Task

```c
void vSensorProcessingTask(void *pvParameters)
{
    uint32_t sensorValue;
    
    for (;;) {
        // Wait for ISR to send data
        if (xQueueReceive(xSensorQueue, &sensorValue, portMAX_DELAY) == pdTRUE) {
            // Heavy processing here
            ProcessVibrationData(sensorValue);
            CheckForAnomalies(sensorValue);
            UpdateStatistics(sensorValue);
        }
    }
}
```

## Best Practices

### DO's
✅ Keep ISRs under 100 instructions
✅ Use `FromISR` API variants
✅ Clear interrupt flags immediately
✅ Defer heavy processing to tasks
✅ Use binary semaphores for simple signaling
✅ Check `xHigherPriorityTaskWoken`

### DON'Ts
❌ Call blocking functions in ISR
❌ Perform complex calculations in ISR
❌ Use floating point in ISR
❌ Allocate memory in ISR
❌ Call non-ISR safe functions

## Performance Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| ISR Execution Time | < 10μs | Hardware timer |
| ISR to Task Latency | < 100μs | Timestamp comparison |
| Interrupt Response Time | < 1μs | Oscilloscope |
| Context Switch Time | < 50μs | Trace analyzer |

## Simulation Considerations

In simulation mode (POSIX port), we simulate interrupts using:
- POSIX signals for timer interrupts
- Thread-based pseudo-interrupts
- Software-triggered events

Note: Real hardware interrupts have different characteristics than simulated ones.

## Testing Strategy

1. **Latency Testing**
   - Measure ISR entry to task execution
   - Verify meets timing requirements

2. **Stress Testing**
   - Generate interrupts at maximum rate
   - Verify no data loss

3. **Priority Testing**
   - Verify high-priority tasks preempt
   - Check deferred task priority

## Common Pitfalls

1. **Stack Overflow**
   - ISRs use system stack
   - Keep local variables minimal

2. **Priority Inversion**
   - Deferred task priority too low
   - Critical data delayed

3. **Interrupt Storm**
   - ISR retriggering too fast
   - System becomes unresponsive

## Code Example

See `examples/02_isr_demo/` for a complete working example demonstrating:
- Timer-based interrupts
- Binary semaphore signaling
- Queue-based data passing
- Latency measurement
- Performance statistics

## Learning Objectives

After completing this capability, you should be able to:
- [ ] Design minimal ISRs
- [ ] Implement deferred processing pattern
- [ ] Use FreeRTOS ISR APIs correctly
- [ ] Measure and optimize ISR latency
- [ ] Handle interrupt priorities
- [ ] Debug interrupt-related issues

## Next Steps

After mastering ISR handling, proceed to:
- Capability 3: Queue-Based Communication
- Integrate ISRs with queue mechanisms
- Build complete sensor pipeline