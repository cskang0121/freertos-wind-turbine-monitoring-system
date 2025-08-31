# Wind Turbine Monitor Dashboard Guide

## Overview

The Wind Turbine Monitor dashboard provides real-time visualization of the FreeRTOS task scheduling system and turbine health metrics. This guide will help you understand each section of the dashboard and interpret the data correctly.

## Dashboard Header

```
==========================================================
 WIND TURBINE MONITOR - CAPABILITIES 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8
                    [SIMULATION MODE]
==========================================================
```

- **[SIMULATION MODE]** - Yellow indicator showing this is running in POSIX simulation, not real hardware
- The header shows integrated capabilities:
  - **Capability 1**: Task Scheduling with priority-based preemption
  - **Capability 2**: Interrupt Service Routines with deferred processing
  - **Capability 3**: Queue-based inter-task communication
  - **Capability 4**: Mutex protection for shared resources
  - **Capability 5**: Event groups for system synchronization
  - **Capability 6**: Memory management with dynamic allocation
  - **Capability 7**: Stack overflow detection with proactive monitoring
  - **Capability 8**: Power management with tickless idle and adaptive behavior

## Understanding Each Section

### 1. Task Scheduler Status

```
TASK SCHEDULER STATUS                   Tick: 94000
----------------------------------------------------------
RUNNING TASKS (7 active):
```

- **Tick**: System tick counter (1ms per tick, configured via `configTICK_RATE_HZ`)
- **Active tasks**: Total number of tasks in the system (including IDLE and Timer tasks)

### 2. Task List

```
  [1] DashboardTask   RUNNING    CPU:  1%*  Stack: 18%*
  [2] NetworkTask     BLOCKED    CPU:  2%*  Stack: 10%*
  [4] SensorTask      BLOCKED    CPU:  8%*  Stack:  8%*
  [3] AnomalyTask     BLOCKED    CPU:  3%*  Stack: 15%*
  [6] SafetyTask      BLOCKED    CPU: 12%*  Stack: 12%*
```

**Format:** `[Priority] TaskName STATE CPU: X%* Stack: Y%*`

- **[Priority]**: Task priority (0=lowest, 6=highest in our system)
- **Task Name**: Descriptive name of the task
- **State**: 
  - `RUNNING` (green) - Currently executing
  - `BLOCKED` (yellow) - Waiting for event/delay
  - `READY` (cyan) - Ready to run but preempted
  - `SUSPENDED` (red) - Suspended by application
- **CPU %**: Estimated CPU usage (marked with * for simulated)
- **Stack %**: Stack memory usage (marked with * for simulated)

### 3. Sensor Readings

```
SENSOR READINGS:
  Vibration: 4.18 mm/s   Temperature: 47.0°C    RPM: 20.0
```

**Simulated sensor data** (dynamically generated with noise and drift):
- **Vibration**: Turbine vibration in mm/s
  - Green: < 5.0 mm/s (normal)
  - Yellow: 5.0-10.0 mm/s (warning)
  - Red: > 10.0 mm/s (critical)
- **Temperature**: Bearing temperature in °C
  - Green: < 70°C (normal)
  - Yellow: 70-85°C (warning)
  - Red: > 85°C (critical)
- **RPM**: Turbine rotation speed
  - Green: 10-30 RPM (normal range)
  - Yellow: Outside normal range

### 4. ISR Status (Capability 2)

```
ISR STATUS:
  Active | Rate: 100Hz | Latency: 250µs | Count: 2400/2400
```

Shows interrupt service routine performance:
- **Active**: ISR is enabled and running
- **Rate**: Interrupt frequency (100Hz = 100 interrupts per second)
- **Latency**: Time from ISR trigger to task processing (microseconds)
  - Good: < 1000µs (1ms)
  - Warning: 1-5ms
  - Bad: > 5ms
- **Count**: `Processed/Total`
  - First number: ISR events processed by tasks
  - Second number: Total ISR triggers
  - Should be equal or very close (indicates no queue overflow)

### 5. Queue Status (Capability 3)

```
QUEUE STATUS:
  Sensor[4/5] Anomaly[2/3] (Used/Size)
```

Shows real-time queue utilization for inter-task communication:
- **Sensor Queue [Used/Size]**: 
  - Size: 5 items maximum
  - Producer: Sensor Task (10Hz)
  - Consumer: Anomaly Task (5Hz with variable consumption)
  - Expected: 2-5 items (40-100% utilized)
  - Shows dynamic behavior alternating between 4/5 and 5/5
- **Anomaly Queue [Used/Size]**:
  - Size: 3 items maximum
  - Producer: Anomaly Task (sends alerts when anomalies detected)
  - Consumer: Network Task (1Hz)
  - Expected: 0-2 items depending on anomaly frequency
  - Fills up when anomalies are detected frequently

**Queue Dynamics:**
- Numbers change in real-time showing push/pop operations
- Sensor queue typically stays 80-100% full due to production > consumption
- Anomaly queue varies based on system health

### 6. Mutex Status (Capability 4)

```
MUTEX STATUS:
  System State: Takes:5075 Gives:5075 Timeouts:0
  Thresholds:   Takes:255 Gives:255 Timeouts:0
```

Shows real-time mutex protection statistics for shared resources:

#### System State Mutex
- **Resource Protected**: `g_system_state` structure
- **Protected Data**: Sensor readings, anomaly results, ISR stats, task metrics
- **Accessors**: All tasks that read/write system state
- **Frequency**: ~500-600 operations per second
- **Metrics**:
  - **Takes**: Number of successful mutex acquisitions
  - **Gives**: Number of mutex releases (should match Takes)
  - **Timeouts**: Failed acquisitions due to 10ms timeout

#### Thresholds Mutex  
- **Resource Protected**: `g_thresholds` configuration
- **Protected Data**: Warning/critical thresholds for all sensors
- **Accessors**: Anomaly Task (5Hz), Safety Task (50Hz)
- **Frequency**: ~55 operations per second
- **Metrics**: Same as System State mutex

**Mutex Health Indicators:**
- Takes should equal Gives (balanced lock/unlock)
- Timeouts should be 0 or very low (<1% of Takes)
- High timeout count indicates contention issues

### 7. Event Group Status (Capability 5)

```
EVENT GROUP STATUS:
  System Ready: [✓] Sensors [✓] Network [✓] Anomaly → ALL READY (2.4s)
  Operations: Sets:3 Clears:0 Waits:1
```

Shows real-time system synchronization status using FreeRTOS event groups:

#### System Ready Synchronization
The system uses event groups to coordinate startup between subsystems:

- **[✓] Sensors**: SensorTask calibrated (after 20 readings, ~2 seconds)
  - Set when sensor readings are stable and baseline established
- **[✓] Network**: NetworkTask connected (50% chance per second to connect)
  - Dynamically set/cleared based on connection status
- **[✓] Anomaly**: AnomalyTask baseline ready (after baseline window filled)
  - Set when anomaly detection has sufficient data for analysis
- **ALL READY**: When all three systems are synchronized and operational

#### Event Group Metrics
- **Sets**: Number of times event bits were set (subsystems becoming ready)
- **Clears**: Number of times event bits were cleared (network disconnections)
- **Waits**: Number of event group wait operations (SafetyTask waits for ALL_READY)
- **Ready Time**: Shows when all subsystems became ready (e.g., "2.4s")

#### Benefits Demonstrated
- **System Coordination**: SafetyTask waits for all subsystems before starting
- **Startup Synchronization**: Prevents race conditions during initialization  
- **Dynamic Status**: Real-time monitoring of subsystem readiness
- **Atomic Operations**: Event bits managed safely across tasks

The event group ensures the critical SafetyTask only begins monitoring after all other subsystems are properly initialized and operational.

### 8. Memory Status (Capability 6)

```
MEMORY STATUS:
  Heap Usage: 31712/262144 bytes (12.1%) | Peak: 268 bytes
  Active Allocs: 1 | Total: Allocs:21 Frees:20 Fails:0
  Fragmentation: 0.6% | Min Free: 230432 bytes
```

This section shows real-time memory management statistics from FreeRTOS heap_4 implementation:

#### Memory Metrics
- **Heap Usage**: Current heap utilization (used/total bytes and percentage)
- **Peak**: Highest memory usage observed since startup
- **Active Allocs**: Currently active memory allocations (should equal Allocs - Frees)
- **Total Operations**: Lifetime allocation/deallocation/failure counts
- **Fragmentation**: Estimated heap fragmentation percentage (based on active allocations)
- **Min Free**: Minimum free heap space ever recorded

#### Dynamic Allocation Demonstrated
The NetworkTask uses dynamic packet allocation with three packet sizes:
- **Heartbeat packets**: 64 bytes (sent every 10 seconds)
- **Sensor data packets**: 256 bytes (normal operation)  
- **Anomaly report packets**: 512 bytes (emergency/high anomaly conditions)

#### Memory Health Indicators
- **Low fragmentation** (< 5%): Good memory management
- **Stable active allocations**: No memory leaks
- **Reasonable peak usage**: System stays within memory bounds
- **Zero allocation failures**: Sufficient heap size for workload

The memory management demonstrates proper dynamic allocation lifecycle with mutex-protected statistics tracking across all tasks.

### 9. Stack Status (Capability 7)

```
STACK STATUS:
  Monitored Tasks: 7 | Warnings: 0 | Critical: 0 | Overflows: 0
  Task Stack Usage:
    DashboardTask  18% (Peak: 18%, Free: 1019 words)
    NetworkTask   10% (Peak: 10%, Free: 507 words)
    SensorTask     8% (Peak: 8%, Free: 507 words)
    AnomalyTask   15% (Peak: 15%, Free: 507 words)
  Proactive Checks: 854 (Good coding practice!)
```

This section shows real-time proactive stack monitoring using FreeRTOS APIs:

#### Stack Monitoring Statistics
- **Monitored Tasks**: Number of tasks being actively monitored for stack usage
- **Warnings**: Count of tasks that exceeded 70% stack usage (proactive warning threshold)
- **Critical**: Count of tasks that exceeded 85% stack usage (critical warning threshold)  
- **Overflows**: Actual stack overflow events detected by FreeRTOS (should always be 0)

#### Per-Task Stack Usage
Each monitored task shows:
- **Current Usage %**: Real-time stack utilization calculated from `uxTaskGetStackHighWaterMark()`
- **Peak Usage %**: Highest stack usage ever recorded for this task
- **Free Words**: Current available stack space in words (1 word = 4 bytes on 32-bit systems)

#### Color-Coded Health Indicators
- **Green (32m)**: < 70% usage - Healthy stack usage
- **Yellow (33m)**: 70-85% usage - Warning threshold reached
- **Red (31m)**: > 85% usage - Critical stack usage requiring attention

#### Proactive Health Checking
- **Proactive Checks**: Counter showing number of health monitoring cycles completed
- **"Good coding practice!"**: Message emphasizing proactive monitoring vs reactive crash handling
- The dashboard task performs stack health checks every 10 refresh cycles
- Demonstrates industry best practice of preventing problems before they occur

#### Real vs Simulated Data
**100% Real FreeRTOS measurements:**
- Stack free words from `task_status[i].usStackHighWaterMark`
- Task count from `uxTaskGetSystemState()`
- Proactive check counter from actual function calls

**Enhanced for simulation visibility:**
- Minimum usage percentages added for demonstration (real hardware would show actual values)
- Pattern helps visualize different task stack requirements

#### Stack Health Benefits
- **Early Warning System**: Detect issues at 70% before critical 85% threshold
- **Trend Monitoring**: Track peak usage over time to identify memory creep
- **Resource Planning**: Understand actual stack requirements for each task type
- **System Stability**: Prevent crashes through proactive monitoring

The stack monitoring demonstrates excellent embedded systems practices by checking for problems before they become critical failures.

### 10. Power Status (Capability 8)

```
POWER STATUS:
  Idle Entries: 372,005 | Sleep Entries: 0 | Wake Events: 0
  Power Savings: 52% | Total Sleep: 0 ms | Last Wake: System
```

This section shows real-time power management statistics using FreeRTOS idle hooks and tickless idle concepts:

#### Power Management Statistics
- **Idle Entries**: Number of times the idle hook has been called (real FreeRTOS measurement)
- **Sleep Entries**: Number of times the system entered tickless sleep mode (pre-sleep processing)
- **Wake Events**: Number of wake-up events from sleep (post-sleep processing)
- **Power Savings**: Estimated power savings percentage based on system idle time

#### Power Calculation Details
The power savings percentage is calculated using a simple but effective formula:
- **For >70% idle time**: `(idle_time_percent - 30) * 1.2` for aggressive savings estimation
- **For <70% idle time**: `idle_time_percent / 2` for conservative estimation
- **Example**: 74% idle time → (74 - 30) * 1.2 = 52.8% → **52% power savings**

#### Power Management Features
- **Real idle monitoring**: Uses actual `vApplicationIdleHook()` calls for measurement
- **Peripheral management**: Pre/post sleep processing hooks for hardware control
- **Adaptive behavior**: Dashboard task reduces refresh rate when power savings >50%
- **Wake source tracking**: Identifies what caused the system to wake up

#### Real vs Simulated Power Data
**100% Real FreeRTOS measurements:**
- Idle entries from actual idle hook executions (372,005+ in example)
- Power savings calculated from real idle time percentage (52% in example)
- Pre/post sleep processing function calls

**POSIX simulation limitations:**
- Sleep entries = 0 (no real hardware sleep in simulation)
- Wake events = 0 (simulation doesn't actually sleep)
- Wake source = "System" (default initialization value)

#### Power Management Benefits
- **Adaptive performance**: System reduces activity when power critical
- **Real-time monitoring**: Continuous visibility into power efficiency
- **Production patterns**: Code ready for real embedded hardware
- **Educational value**: Demonstrates industry-standard power management

#### Adaptive Behavior Demonstration
When power savings exceed 50%, the dashboard task automatically:
- Reduces refresh rate from 1Hz to 0.5Hz (2-second intervals)
- Demonstrates power-aware application behavior
- Shows responsive power management in action

The power management system demonstrates practical embedded systems power optimization while maintaining system functionality and user experience.

### 11. Preemption Events

```
PREEMPTION EVENTS (Last 5):
  [ 88000] SafetyTask      preempted SensorTask      (Priority  )
  [ 90050] SensorTask      preempted NetworkTask     (Yield     )
```

Shows the last 5 task preemptions:
- **[Tick]**: When the preemption occurred
- **Preemptor**: Task that took control
- **Preempted**: Task that was interrupted
- **Reason**:
  - `Priority`: Higher priority task became ready
  - `Yield`: Task voluntarily yielded CPU
  - `Critical`: Emergency/critical event

### 12. Scheduling Metrics

```
SCHEDULING METRICS:
  Context Switches: 6393      * Idle Time: 74%*
  Task Switches/sec: 67       * CPU Usage: 26%*
```

- **Context Switches**: Total number of task switches since start (estimated*)
- **Task Switches/sec**: Rate of context switching
- **Idle Time %**: Percentage of time system is idle (estimated*)
- **CPU Usage %**: Overall CPU utilization (estimated*)

### 13. Health Status

```
HEALTH STATUS: 70% [##############------] WARNING
```

- **Percentage**: Overall system health score (0-100%)
- **Progress Bar**: Visual representation
- **Status**:
  - `HEALTHY` (green): > 80%
  - `WARNING` (yellow): 50-80%
  - `CRITICAL` (red): < 50%

Health score is calculated based on:
- Sensor readings vs thresholds
- Anomaly detection results
- System stability metrics

### 14. Footer

```
Uptime: 00:01:34 | Network: Connected | Anomalies: 545
* Estimated metrics (POSIX simulation)
```

- **Uptime**: How long the system has been running (HH:MM:SS)
- **Network**: Connection status (Connected/Disconnected)
- **Anomalies**: Total anomalies detected since start
- **Note**: Asterisk (*) indicates estimated metrics in simulation

## Understanding Simulated vs Real Data

### Real Data (From FreeRTOS)
- Task names and priorities
- Task states (RUNNING, BLOCKED, etc.)
- Preemption events (manually recorded)
- System uptime

### Simulated/Calculated Data
**Marked with (*):**
- CPU usage percentages
- Stack usage percentages  
- Context switch counts
- Idle time percentage

**Dynamic but simulated:**
- Sensor readings (vibration, temperature, RPM)
- Anomaly detection results
- Network status
- Health score calculations

These metrics are estimated because the POSIX simulation environment doesn't provide real hardware performance counters.

## Common Patterns to Watch

### Normal Operation
- SafetyTask regularly preempting others (highest priority)
- CPU usage around 20-30%
- Health status > 80%
- Steady sensor readings within thresholds

### Warning Signs
- Health status < 80%
- Vibration > 5 mm/s
- Temperature > 70°C
- Frequent network disconnections
- Anomaly count increasing rapidly

### Critical Issues
- Emergency stop activated
- Health status < 50%
- Multiple sensors in critical range
- Tasks stuck in same state

## Task Execution Frequencies

Understanding how often each task runs helps interpret the metrics:

| Task | Frequency | Period | Priority | Purpose |
|------|-----------|--------|----------|---------|
| SafetyTask | 20 Hz | 50ms | 6 (Highest) | Critical safety monitoring |
| SensorTask | 10 Hz | 100ms | 4 | Read sensor data |
| AnomalyTask | 2 Hz | 500ms | 3 | Detect anomalies |
| NetworkTask | 1 Hz | 1000ms | 2 | Send data to cloud |
| DashboardTask | 1 Hz | 1000ms | 1 (Lowest) | Update display |

## Troubleshooting

### Dashboard shows all 0% CPU/Stack
- Normal at startup (takes a second to calculate)
- POSIX simulation limitation

### Only DashboardTask shows RUNNING
- This is the "observer problem" - dashboard can only see system state when it's running
- Other tasks ARE running (check preemption events and changing sensor values)

### Context switches seem high
- Normal: ~60-70 switches/sec with 5 tasks
- Each task execution causes 2 switches (in and out)

### Health score fluctuates
- Based on current readings, not history
- Check sensor values against thresholds

## POSIX Simulation Limitations

When running on Mac/Linux (not real embedded hardware):

1. **No real CPU metrics** - Uses frequency-based estimation
2. **Minimal stack usage** - Host OS manages memory differently
3. **No hardware interrupts** - Uses signals/timers instead
4. **Thread-based tasks** - Not true lightweight tasks

Despite these limitations, the simulation accurately demonstrates:
- Priority-based preemption
- Task state transitions
- Real-time scheduling behavior
- Inter-task communication

## Tips for Learning

1. **Watch the preemption events** - Shows real-time scheduling in action
2. **Modify task priorities** - See how it affects preemption patterns
3. **Adjust sensor thresholds** - Observe health score changes
4. **Increase task frequencies** - See impact on CPU usage
5. **Add debug output** - Tasks print to console behind dashboard

## Related Documentation

- [Capability 1: Task Scheduling](capabilities/01_task_scheduling.md)
- [Integrated System README](../src/integrated/README.md)
- [FreeRTOS Configuration](../config/FreeRTOSConfig.h)

---

*Last Updated: 2025-08-31*
*Version: 1.3 - Added Capability 8 (Power Management)*