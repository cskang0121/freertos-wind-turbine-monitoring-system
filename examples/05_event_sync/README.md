# Example 05: Event Groups for Complex Synchronization

## Overview

This example demonstrates the power of FreeRTOS Event Groups for complex multi-condition synchronization. It simulates a wind turbine control system where multiple tasks coordinate based on various system events, alarms, and operational conditions.

## System Architecture

```
Event Groups Structure:
                                    
System Events (Initialization)     Operational Events              Safety Events
---------------------------        -------------------             -------------
Bit 0: WIFI_CONNECTED             Bit 8:  ANOMALY_DETECTED       Bit 16: MAINTENANCE_MODE
Bit 1: SENSORS_READY              Bit 9:  DATA_READY             Bit 17: EMERGENCY_STOP
Bit 2: CONFIG_LOADED              Bit 10: BUFFER_FULL            Bit 18: OVERSPEED_ALARM
Bit 3: SYSTEM_INITIALIZED         Bit 11: TRANSMISSION_DONE      Bit 19: VIBRATION_ALARM

Task Dependencies:
Init Task ──────> Sets system bits sequentially
                  Waits for ALL bits (AND logic)
                  
Sensor Task ────> Waits for SYSTEM_INITIALIZED
                  Generates anomalies
                  Sets DATA_READY periodically
                  
Network Task ───> Waits for DATA_READY OR ANOMALY
                  Checks WIFI_CONNECTED
                  Prioritizes anomaly transmission
                  
Safety Task ────> Waits for ANY alarm (OR logic)
                  Triggers EMERGENCY_STOP on multiple alarms
                  Clears alarms after handling
```

## Key Demonstrations

### 1. AND Logic - System Initialization

The Init Task demonstrates waiting for multiple conditions:

```c
// Wait for ALL initialization components
EventBits_t bits = xEventGroupWaitBits(
    xSystemEvents,
    WIFI_CONNECTED_BIT | SENSORS_READY_BIT | CONFIG_LOADED_BIT,
    pdFALSE,  // Don't clear
    pdTRUE,   // AND logic - need ALL bits
    timeout
);
```

System only proceeds when:
- WiFi is connected AND
- Sensors are ready AND  
- Config is loaded

### 2. OR Logic - Alarm Handling

The Safety Task responds to any alarm condition:

```c
// Wait for ANY alarm
EventBits_t alarms = xEventGroupWaitBits(
    xSafetyEvents,
    OVERSPEED_ALARM_BIT | VIBRATION_ALARM_BIT,
    pdFALSE,
    pdFALSE,  // OR logic - ANY alarm triggers
    timeout
);
```

Task wakes when:
- Overspeed detected OR
- High vibration detected

### 3. Event Broadcasting

When an event is set, ALL waiting tasks wake simultaneously:

```c
// Sensor detects anomaly
xEventGroupSetBits(xOperationalEvents, ANOMALY_DETECTED_BIT);

// Both Network and Monitor tasks wake up
// Network: Transmits alert
// Monitor: Logs event
```

### 4. Synchronization Barrier

Three sync tasks demonstrate barrier synchronization:

```c
// Each task contributes its bit and waits for all
xEventGroupSync(
    xSyncEvents,
    TASK_A_SYNC_BIT,  // Our contribution
    ALL_SYNC_BITS,    // Wait for all tasks
    timeout
);
```

All tasks proceed only when everyone reaches the barrier.

## Tasks Description

### 1. Init Task (Priority 5)
- Initializes system components sequentially
- Sets event bits as each component ready
- Waits for complete system initialization
- Runs once then deletes itself

### 2. Sensor Task (Priority 4)
- Simulates vibration and speed monitoring
- Generates anomalies based on thresholds
- Sets DATA_READY every 10 samples
- Triggers alarms on dangerous conditions

### 3. Network Task (Priority 3)
- Waits for data or anomalies
- Checks WiFi connectivity
- Prioritizes anomaly transmission
- Clears events after transmission

### 4. Safety Task (Priority 6 - Highest)
- Monitors all alarm conditions
- Responds immediately to safety events
- Triggers emergency stop on multiple alarms
- Highest priority for safety-critical response

### 5. Maintenance Task (Priority 2)
- Periodically enters maintenance mode
- Sets/clears maintenance bit
- Simulates scheduled maintenance windows

### 6. Sync Tasks A/B/C (Priority 3)
- Demonstrate barrier synchronization
- Each reaches sync point at different times
- All proceed together when synchronized

### 7. Monitor Task (Priority 1)
- Displays event group states
- Shows bit patterns visually
- Tracks statistics
- Updates every 5 seconds

## Expected Output

```
===========================================
Example 05: Event Groups
Complex multi-condition synchronization
===========================================

[INIT] Starting system initialization...
[INIT] Loading configuration...
[INIT] Initializing sensors...
[SENSOR] Task started
[NETWORK] Task started
[SAFETY] Monitor started
[INIT] Connecting to WiFi...
[INIT] System fully initialized!
[EVENTS] System Events: 0x00000F = 0000 1111

[SENSOR] System ready, starting monitoring
[SENSOR] High vibration detected: 68
[SAFETY] ALARM CONDITIONS DETECTED:
  - Vibration limit exceeded
[NETWORK] Transmitting PRIORITY anomaly data...

[SYNC-A] Reaching synchronization point...
[SYNC-B] Reaching synchronization point...
[SYNC-C] Reaching synchronization point...
[SYNC-A] All tasks synchronized!
[SYNC-B] All tasks synchronized!
[SYNC-C] All tasks synchronized!

========================================
EVENT GROUP STATUS
========================================
[EVENTS] System: 0x00000F = 0000 1111
  WiFi: YES, Sensors: YES, Config: YES, Init: YES
[EVENTS] Operational: 0x000200 = 0010 0000 0000
  Anomaly: NO, Data: YES, Buffer: NO, Tx: NO
[EVENTS] Safety: 0x000000 = 0000 0000 0000 0000 0000 0000
  Maintenance: NO, Emergency: NO
  Overspeed: NO, Vibration: NO

Statistics:
  Events Set:      15
  Events Cleared:  8
  Anomalies:       3
  Transmissions:   5
  Emergency Stops: 0
  Timeouts:        0
========================================
```

## Learning Points

### 1. Event Group Advantages

| Use Case | Event Groups | Alternative | Why Event Groups Better |
|----------|--------------|-------------|-------------------------|
| Multiple conditions | Single wait call | Multiple semaphores | Atomic, efficient |
| Broadcast event | All tasks wake | Queue to each task | Simpler, faster |
| State machine | Bit patterns | Multiple variables | Thread-safe, compact |
| System ready | AND logic built-in | Complex semaphore logic | Cleaner code |

### 2. Bit Management

```c
// Setting multiple bits atomically
xEventGroupSetBits(xEvents, BIT_A | BIT_B | BIT_C);

// Checking specific bits
if (bits & MY_BIT) { /* bit is set */ }

// Checking exact match
if ((bits & REQUIRED_BITS) == REQUIRED_BITS) { /* all required bits set */ }

// Toggle bit (clear then set)
xEventGroupClearBits(xEvents, MY_BIT);
xEventGroupSetBits(xEvents, MY_BIT);
```

### 3. Clear Strategy

**Auto-clear** for consumed events:
```c
xEventGroupWaitBits(xEvents, DATA_READY, 
                    pdTRUE,  // Clear after reading
                    pdTRUE, pdTRUE, timeout);
```

**Don't clear** for persistent states:
```c
xEventGroupWaitBits(xEvents, SYSTEM_READY,
                    pdFALSE,  // Leave set
                    pdTRUE, pdTRUE, timeout);
```

### 4. ISR Limitations

```c
// ISR can only set/clear, not wait
void My_ISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Set event from ISR (deferred to timer task)
    xEventGroupSetBitsFromISR(xEvents, ISR_EVENT_BIT,
                              &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Exercises

1. **Add Critical Event**
   - Define a CRITICAL_FAILURE_BIT
   - Make it trigger immediate emergency stop
   - Require manual reset

2. **Implement Timeout Handling**
   - Add timeout to network transmission wait
   - Log missed events
   - Implement retry logic

3. **Create State Machine**
   - Define states as bit combinations
   - Implement state transitions
   - Prevent invalid states

4. **Add Event History**
   - Track last 10 events
   - Include timestamps
   - Display in monitor

5. **Performance Monitoring**
   - Measure event latency
   - Track wake time from event set
   - Optimize event handling

## Wind Turbine Application

This example directly models a wind turbine control system:

1. **Initialization Sequence**
   - Sensors must calibrate
   - Network must connect
   - Config must load
   - Only then can operation begin

2. **Anomaly Detection**
   - Vibration monitoring
   - Speed monitoring
   - Immediate alert transmission
   - Safety response

3. **Maintenance Windows**
   - Scheduled maintenance mode
   - Prevents normal operation
   - Coordinated system pause

4. **Emergency Response**
   - Multiple alarm conditions
   - Immediate safety action
   - Broadcast to all subsystems

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/05_event_sync/event_sync_example

# Observe:
# - Initialization sequence
# - Anomaly detection and response
# - Synchronization barriers
# - Event broadcasting
```

## Key Takeaways

1. **Event Groups excel at multi-condition synchronization**
2. **AND/OR logic built into API**
3. **Broadcasting wakes all waiting tasks**
4. **24 bits available per event group**
5. **Perfect for state machines and system coordination**
6. **Cannot be used from ISRs for waiting**
7. **Atomic operations prevent race conditions**

## Integration with Final System

This event synchronization capability is vital for the integrated Wind Turbine Monitor:

### Direct Applications

1. **System State Management**
   - Track initialization of all subsystems
   - Coordinate startup sequence
   - Manage operational modes

2. **Multi-Condition Monitoring**
   - Wait for complex alarm combinations
   - Trigger emergency stop on multiple faults
   - Coordinate maintenance windows

3. **Event Broadcasting**
   - Notify all tasks of critical events
   - System-wide mode changes
   - Emergency notifications

### Console Dashboard Preview

```
EVENT STATUS                          Active Events: 5/24
----------------------------------------------------------
SYSTEM EVENTS:
  [X] WiFi Connected    [X] Sensors Ready
  [X] Config Loaded     [X] System Init
  
OPERATIONAL EVENTS:
  [ ] Anomaly Detected  [X] Data Ready
  [ ] Buffer Full       [X] Transmission Done
  
SAFETY EVENTS:
  [ ] Maintenance Mode  [ ] Emergency Stop
  [!] Overspeed Alarm   [ ] Vibration Alarm
  
Waiting Tasks: 3      Last Event: OVERSPEED_ALARM (2s ago)
```

## Next Steps

After mastering event groups, proceed to:
- Example 06: Memory Management with Heap_4
- Then continue through remaining capabilities
- Finally integrate all 8 capabilities into complete system