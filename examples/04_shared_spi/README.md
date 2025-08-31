# Example 04: Mutex for Shared Resources

## Overview

This example demonstrates proper mutex usage for protecting shared resources in a multi-tasking environment. Multiple tasks with different priorities access a shared SPI bus to communicate with various sensors, showcasing:

- Mutex-based resource protection
- Priority inheritance to prevent priority inversion
- Recursive mutexes for nested function calls
- Configuration data protection
- Deadlock prevention strategies

## System Architecture

```
High Priority Tasks                    Shared Resources
-------------------                    ----------------
Vibration (Pri 6) ─┐                   
                   ├──> SPI Bus Mutex ──> SPI Hardware
Current (Pri 5) ───┤    (Priority         (4 Sensors)
                   │     Inheritance)
Temperature (Pri 4)┘                   
                                       
Config (Pri 3) ────────> Config Mutex ──> Config Data
                                       
Pressure (Pri 2) ──────> SPI Bus Mutex    
                         (May experience
                          delays)

All Tasks ─────────────> Recursive ────> Logging
                         Mutex            System
```

## Components

### Tasks

1. **Vibration Task (Priority 6)**
   - Highest priority - safety critical
   - 100Hz sampling rate
   - Monitors vibration sensor
   - Generates alerts on threshold

2. **Current Task (Priority 5)**
   - High priority
   - 50Hz sampling rate
   - Monitors motor current
   - Detects overcurrent conditions

3. **Temperature Task (Priority 4)**
   - Medium priority
   - 10Hz sampling rate
   - Monitors bearing temperature
   - Thermal protection

4. **Configuration Task (Priority 3)**
   - Low-medium priority
   - Updates thresholds periodically
   - Demonstrates config mutex

5. **Pressure Task (Priority 2)**
   - Low priority
   - 1Hz sampling rate
   - May experience delays
   - Shows priority inheritance benefit

6. **Statistics Task (Priority 1)**
   - Lowest priority
   - Reports system statistics
   - Monitors mutex performance

### Mutexes

1. **SPI Mutex**
   - Protects SPI bus hardware
   - Enables priority inheritance
   - Prevents data corruption
   - Tracks timeout events

2. **Configuration Mutex**
   - Protects shared config structure
   - Ensures atomic updates
   - Prevents partial reads

3. **Recursive Mutex**
   - Allows nested function calls
   - Same task can take multiple times
   - Used for logging system

## Key Features Demonstrated

### 1. Priority Inheritance

```c
// Without priority inheritance:
// High(6) blocked -> Medium(4) runs -> Low(2) holds mutex = BAD!

// With priority inheritance (mutex):
// High(6) blocked -> Low(2) boosted to 6 -> Medium(4) cannot run = GOOD!
```

### 2. Timeout Handling

```c
if (xSemaphoreTake(xSPIMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    // Got mutex - access SPI
    spi_transfer(sensor, cmd, data, len);
    xSemaphoreGive(xSPIMutex);
} else {
    // Timeout - handle gracefully
    g_stats.mutex_timeouts++;
}
```

### 3. Recursive Mutex Usage

```c
// High level function
high_level_log() {
    xSemaphoreTakeRecursive(mutex);
    low_level_log();  // Nested call - OK!
    xSemaphoreGiveRecursive(mutex);
}

// Low level function
low_level_log() {
    xSemaphoreTakeRecursive(mutex);  // Same task, works!
    // Do logging
    xSemaphoreGiveRecursive(mutex);
}
```

## Expected Output

```
===========================================
Example 04: Mutex for Shared Resources
Multiple tasks sharing SPI bus
Demonstrates priority inheritance
===========================================

[MAIN] Mutexes created successfully
  - SPI Mutex (with priority inheritance)
  - Config Mutex (protects shared data)
  - Recursive Mutex (allows nested calls)

[VIBRATION] Task started (Priority 6, 100Hz)
[CURRENT] Task started (Priority 5, 50Hz)
[TEMPERATURE] Task started (Priority 4, 10Hz)
[CONFIG] Task started (Priority 3)
[PRESSURE] Task started (Priority 2, 1Hz)

[SPI] Sensor 0: CMD=0x01 Vibration=11523
[SPI] Sensor 2: CMD=0x03 Current=12A
[LOG-LL] CURRENT: High current detected
[SPI] Sensor 1: CMD=0x02 Temp=25

[CONFIG] Updated: Vib=105, Temp=85

[PRESSURE] Delayed by 15 ms (priority inversion?)

========================================
MUTEX SYSTEM STATISTICS
========================================
SPI Transactions:    1250
  Vibration:         500
  Temperature:       50
  Current:           250
  Pressure:          5

Mutex Performance:
  Timeouts:          0
  Max Wait:          8 ticks
  Priority Inversions: 2

Config Updates:      3
========================================
```

## Performance Analysis

### Mutex Overhead

- **Take operation**: ~100-200 CPU cycles
- **Give operation**: ~100-200 CPU cycles  
- **Priority inheritance**: Additional ~50 cycles
- **Recursive tracking**: Additional ~20 cycles

### Critical Section Timing

| Task | Critical Section | Frequency | Bus Usage |
|------|-----------------|-----------|-----------|
| Vibration | 2ms | 100Hz | 20% |
| Current | 2ms | 50Hz | 10% |
| Temperature | 2ms | 10Hz | 2% |
| Pressure | 2ms | 1Hz | 0.2% |
| **Total** | - | - | **32.2%** |

### Priority Inheritance Benefits

Without priority inheritance:
- Low priority task holds mutex
- High priority task blocks
- Medium priority tasks run
- High priority task delayed significantly

With priority inheritance:
- Low priority task boosted to high priority
- Completes critical section quickly
- High priority task resumes sooner
- System responsiveness improved

## Learning Points

### 1. When to Use Mutex vs Semaphore

**Use Mutex when:**
- Protecting shared resources
- Need priority inheritance
- Same task takes and gives
- Preventing priority inversion

**Use Binary Semaphore when:**
- Signaling between tasks
- ISR to task communication
- Different tasks take/give
- Simple synchronization

### 2. Deadlock Prevention Rules

1. **Always acquire in same order**
   ```c
   // All tasks: First SPI, then Config
   xSemaphoreTake(xSPIMutex, timeout);
   xSemaphoreTake(xConfigMutex, timeout);
   ```

2. **Always use timeouts**
   ```c
   // Never use portMAX_DELAY
   xSemaphoreTake(mutex, pdMS_TO_TICKS(1000));
   ```

3. **Release before taking another**
   ```c
   xSemaphoreGive(mutex1);
   xSemaphoreTake(mutex2, timeout);
   ```

### 3. Critical Section Guidelines

**DO:**
- Keep critical sections short
- Only protect actual shared access
- Use local copies when possible
- Release mutex on all code paths

**DON'T:**
- Perform calculations inside mutex
- Call blocking functions inside mutex
- Use mutex from ISR
- Forget to give mutex back

## Exercises

1. **Increase Load**
   - Change vibration to 200Hz
   - Observe timeout occurrences
   - Find maximum sustainable rate

2. **Create Deadlock**
   - Add second mutex
   - Take in different order
   - Observe system freeze
   - Fix with proper ordering

3. **Measure Priority Inheritance**
   - Add priority monitoring
   - Log when priority changes
   - Measure boost duration

4. **Optimize Critical Sections**
   - Reduce time in mutex
   - Use local buffers
   - Measure improvement

5. **Add Queue Communication**
   - Combine with queues from Example 03
   - Protect queue with mutex
   - Compare with built-in queue protection

## Wind Turbine Application

This example directly models the wind turbine system:

1. **Vibration Monitoring** (Critical)
   - Highest priority for safety
   - 100Hz accelerometer data
   - Immediate response needed

2. **Current Monitoring** (Important)
   - Motor protection
   - 50Hz sampling
   - Overcurrent detection

3. **Temperature Tracking** (Normal)
   - Bearing health
   - 10Hz is sufficient
   - Predictive maintenance

4. **Pressure Monitoring** (Background)
   - Environmental data
   - Low priority
   - Can tolerate delays

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/04_shared_spi/shared_spi_example

# Observe:
# - No mutex timeouts (good design)
# - Priority inversions (without inheritance)
# - Maximum wait times
# - Fair resource sharing
```

## Troubleshooting

### Problem: Mutex Timeouts

**Symptoms**: Tasks report timeout errors

**Solutions**:
- Increase timeout values
- Reduce critical section time
- Lower sampling rates
- Add more mutexes (one per resource)

### Problem: Priority Inversions

**Symptoms**: High priority tasks delayed

**Solutions**:
- Ensure using mutex (not binary semaphore)
- Verify priority inheritance enabled
- Check for nested mutex usage
- Review task priorities

### Problem: Deadlock

**Symptoms**: System freezes

**Solutions**:
- Always use timeouts
- Acquire mutexes in same order
- Use recursive mutex for nesting
- Add deadlock detection

## Integration with Final System

This mutex protection capability is essential for the integrated Wind Turbine Monitor:

### Direct Applications

1. **SPI Bus Management**
   - Multiple sensors share single SPI bus
   - Mutex prevents data corruption
   - Priority inheritance ensures responsiveness

2. **Configuration Protection**
   - Threshold values need atomic updates
   - Multiple tasks read configuration
   - Prevents partial reads during updates

3. **Critical Section Management**
   - Hardware register access
   - Shared data structure updates
   - Resource pool management

### Console Dashboard Preview

```
RESOURCE MONITOR                      Mutex Status: ACTIVE
----------------------------------------------------------
SPI BUS:
  Owner: VibrationTask    Priority: 6 (inherited)
  Waiting: 2 tasks        Max Wait: 8ms
  
CONFIG DATA:
  Status: LOCKED          Owner: ConfigTask
  Readers Waiting: 0      Last Update: 2.3s ago
  
CRITICAL SECTIONS:
  Active: 3/8             Avg Duration: 1.2ms
  Timeouts: 0             Priority Inversions: 0
```

## Next Steps

After mastering mutexes, proceed to:
- Example 05: Event Groups for Complex Synchronization
- Then continue through remaining capabilities
- Finally integrate all 8 capabilities into complete system