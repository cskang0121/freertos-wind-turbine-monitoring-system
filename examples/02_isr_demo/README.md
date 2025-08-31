# Example 02: Interrupt Service Routines with Deferred Processing

## Overview

This example demonstrates proper interrupt handling in FreeRTOS using the deferred processing pattern. It simulates a 100Hz timer interrupt that triggers sensor readings, similar to what would be used in our wind turbine predictive maintenance system.

## What You'll Learn

1. **Minimal ISR Design** - Keep ISRs under 100 instructions
2. **FromISR API Usage** - Proper FreeRTOS APIs for ISR context
3. **Deferred Processing** - Move heavy work to high-priority tasks
4. **Binary Semaphores** - Simple ISR to task signaling
5. **Queue Communication** - Pass data from ISR to tasks
6. **Latency Measurement** - Track ISR response times
7. **Emergency Handling** - Priority-based critical event processing

## Key Components

### Simulated ISR (`vSimulatedISR`)
- Triggered by 100Hz timer
- Reads sensor value (minimal processing)
- Sends data via queue
- Checks for emergency conditions
- Requests context switch when needed

### Deferred Processing Task
- Priority 6 (high)
- Receives sensor data from queue
- Performs heavy calculations
- Measures and reports latency
- Detects anomalies

### Emergency Response Task
- Priority 7 (highest)
- Handles critical events
- Minimal latency response
- Would trigger emergency stop in real system

### Monitor Task
- Reports statistics every 5 seconds
- Tracks interrupt count, latency, dropped events
- Calculates processing rate

## Expected Output

```
===========================================
Example 02: ISR with Deferred Processing
Simulating 100Hz timer interrupts
===========================================

[EMERGENCY] Task started (Priority 7) - CRITICAL
[DEFERRED] Task started (Priority 6)
[MONITOR] Task started - Reports every 5 seconds

[DEFERRED] Processing sensor data #0: value=125, latency=1000us
[DEFERRED] Warning: High vibration detected: 125
[DEFERRED] Processing sensor data #1: value=89, latency=900us
[DEFERRED] Processing sensor data #2: value=156, latency=1100us
[DEFERRED] Warning: High vibration detected: 156

*** [EMERGENCY] CRITICAL EVENT ***
*** Vibration level: 156 ***
*** Initiating emergency response ***
*** Latency: 2 ticks ***

========================================
ISR STATISTICS REPORT
========================================
Total Interrupts:    500
Processed Events:    498
Dropped Events:      2
Max Latency:         2000 us
Min Latency:         800 us
Avg Latency:         1050 us
Processing Rate:     99.6%
========================================
```

## Code Structure

```
vSimulatedISR()              [ISR Context]
    |
    ├── Read sensor value    (< 10 instructions)
    ├── Check emergency      (< 5 instructions)
    ├── Queue send          (< 20 instructions)
    └── Context switch      (< 5 instructions)
         |
         v
vDeferredProcessingTask()    [Task Context]
    |
    ├── Receive from queue
    ├── Measure latency
    ├── Heavy processing     (1000+ instructions)
    └── Anomaly detection
```

## Performance Metrics

| Metric | Target | Typical | Notes |
|--------|--------|---------|-------|
| ISR Execution | < 10μs | 5μs | Minimal processing only |
| ISR to Task Latency | < 1ms | 900μs | High priority task |
| Emergency Response | < 100μs | 50μs | Critical path |
| Processing Rate | > 99% | 99.6% | Queue sized appropriately |

## Wind Turbine Application

In our real system, this pattern would handle:

1. **Vibration Monitoring**
   - 100Hz sampling rate
   - Immediate anomaly detection
   - Emergency stop on critical levels

2. **RPM Sensing**
   - Blade rotation monitoring
   - Overspeed protection
   - Performance optimization

3. **Temperature Alerts**
   - Bearing temperature monitoring
   - Overheating prevention
   - Predictive maintenance triggers

## Key Takeaways

1. **ISRs must be minimal** - Do only what's absolutely necessary
2. **Use FromISR APIs** - Regular APIs will crash in ISR context
3. **High priority for deferred tasks** - Ensures low latency
4. **Queue sizing matters** - Too small = data loss, too large = memory waste
5. **Measure everything** - Latency tracking is crucial for real-time systems

## Exercises

1. **Modify ISR frequency** - Change timer to 1kHz and observe impact
2. **Add more sensors** - Simulate multiple interrupt sources
3. **Implement filtering** - Add moving average in deferred task
4. **Test queue overflow** - Reduce queue size and observe behavior
5. **Priority experiments** - Change task priorities and measure latency

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/02_isr_demo/isr_demo_example

# Watch for:
# - Latency measurements
# - Emergency events
# - Statistics reports
# - Processing rate
```

## Integration with Final System

### How This Capability Is Used
In the complete wind turbine monitoring system, ISRs provide critical real-time response:
- **Vibration ISR**: Triggers on dangerous vibration levels
- **Emergency Stop ISR**: Hardware button for immediate shutdown
- **Timer ISR**: Periodic sensor sampling trigger
- **Deferred to SafetyTask**: Complex processing done in task context

### Console Dashboard Preview
In the integrated system, you'll see ISR performance metrics:
```
ISR PERFORMANCE:
  Source       Count    Avg Latency   Max Latency   Deferred To
  Vibration    1245     45 μs         89 μs         SafetyTask
  Emergency    2        12 μs         15 μs         SafetyTask
  Timer        10000    23 μs         67 μs         SensorTask
  
RECENT ISR EVENTS:
  [10:23:45.001] Vibration ISR triggered (2.8 mm/s)
  [10:23:45.002] Semaphore given to SafetyTask
  [10:23:45.003] SafetyTask processing (latency: 45 μs)
  [10:23:45.010] Processing complete, no action needed
```

## Next Steps

After understanding ISR handling, proceed to:
- Example 03: Queue-Based Producer-Consumer
- Combine ISRs with advanced queue patterns
- Build complete sensor data pipeline