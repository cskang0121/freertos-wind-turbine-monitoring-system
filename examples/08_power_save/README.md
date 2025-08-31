# Example 08: Power Management with Tickless Idle

## Overview

This example demonstrates FreeRTOS tickless idle mode for power management, showing how to dramatically reduce power consumption in battery-powered IoT devices while maintaining real-time responsiveness.

## Key Features Demonstrated

### 1. Tickless Idle Operation
- Dynamic tick suppression
- Sleep during idle periods
- Automatic wake on events
- Power state transitions

### 2. Power Profiles
- High Performance (0% saving)
- Balanced (30% saving target)
- Power Saver (60% saving target)
- Ultra Low Power (80% saving target)

### 3. Wake Source Management
- Timer-based wake (periodic tasks)
- Event-driven wake (network/sensor)
- Alarm wake (critical events)
- Wake source tracking

### 4. Battery-Aware Operation
- Battery level monitoring
- Automatic profile selection
- Power consumption estimation
- Battery life prediction

## System Architecture

```
Power State Machine:
┌─────────┐  Activity  ┌─────────┐
│   RUN   │◄──────────►│  IDLE   │
└─────────┘            └─────────┘
     ▲                      │
     │                      │ No tasks ready
     │ Wake event           ▼
     │                 ┌─────────┐
     └─────────────────│  SLEEP  │
                      └─────────┘

Task Activity Pattern:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
│Task│  IDLE  │Task│    SLEEP    │Task│ SLEEP │
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Active         Sleep for         Wake  Extended
 period         calculated        event   sleep
                  time

Power Profiles:
┌────────────────┬──────────┬─────────┬────────┐
│ Profile        │ Interval │ Network │ Target │
├────────────────┼──────────┼─────────┼────────┤
│ High Perf      │ 1 sec    │ Yes     │ 0%     │
│ Balanced       │ 5 sec    │ Yes     │ 30%    │
│ Power Saver    │ 30 sec   │ No      │ 60%    │
│ Ultra Low      │ 60 sec   │ No      │ 80%    │
└────────────────┴──────────┴─────────┴────────┘
```

## Tasks Description

### 1. Sensor Task (Priority 3)
- Periodic sensor readings
- Configurable interval based on profile
- Generates SENSOR_DATA_READY events
- Primary timer-based wake source

### 2. Network Task (Priority 2)
- Event-driven packet processing
- Can be disabled in power-saving modes
- Handles data transmission
- Network-based wake source

### 3. Logger Task (Priority 1)
- Batch log processing
- Reduces write operations
- Flushes every 30 seconds or when full
- Minimizes active time

### 4. Alarm Task (Priority 4)
- Handles critical events
- Low battery warnings
- System alarms
- Always-enabled wake source

### 5. Monitor Task (Priority 1)
- Tracks power statistics
- Reports every 10 seconds
- Calculates power savings
- Adjusts profiles based on battery

### 6. Activity Task (Priority 1)
- Simulates system activity
- Creates busy/idle patterns
- Tests sleep/wake cycles
- Demonstrates power savings

## Expected Output

```
===========================================
Example 08: Power Management
Tickless Idle Demonstration
===========================================

Features:
  - Dynamic power profiles
  - Battery-aware operation
  - Multiple wake sources
  - Power consumption tracking
  - Idle time optimization

NOTE: This example simulates tickless idle
      Real implementation requires hardware support

[PROFILE] Applying 'Balanced' profile
  Sensor interval: 5000 ms
  Network: Enabled
  Target saving: 30%

[SENSOR] Task started
[NETWORK] Task started
[LOGGER] Task started
[ALARM] Task started
[MONITOR] Task started
[ACTIVITY] Simulation task started

[SENSOR] Reading #1 at tick 100
[NETWORK] Transmitting sensor data
[ACTIVITY] Busy period 0
[ACTIVITY] Idle period 0
[POWER] Entering sleep for 2000 ms
[POWER] Woke after 2000 ms (source: 0)

========================================
POWER MANAGEMENT STATISTICS
========================================
Current Profile:     Balanced
Total Runtime:       10000 ms
Idle Time:          3500 ms
Power Saving:       35.0% (target: 30%)
Sleep Count:        5
Last Sleep:         2000 ms
Longest Sleep:      2000 ms

Wake Sources:
  Timer:            3
  Network:          1
  Sensor:           0
  Alarm:            1
  Unknown:          0

Battery Status:
  Voltage:          4100 mV
  Level:            92%
  Charging:         No
  Estimated Life:   18.4 hours
========================================

[PROFILE] Applying 'Power Saver' profile
  Sensor interval: 30000 ms
  Network: Disabled
  Target saving: 60%

[MONITOR] Entering extended idle period...
[POWER] Entering sleep for 5000 ms
[POWER] Woke after 5000 ms (source: 0)
```

## Power Saving Mechanisms

### 1. Tick Suppression
```c
// Traditional: Tick interrupt every 1ms
// Tickless: No ticks during idle
// Savings: Reduces CPU wake-ups by 90%+
```

### 2. Peripheral Management
```c
// Before sleep: Disable unused peripherals
// After wake: Re-enable only what's needed
// Savings: 20-40mA reduction
```

### 3. Dynamic Frequency Scaling
```c
// High load: 200MHz
// Medium load: 100MHz
// Low load: 50MHz
// Sleep: 32kHz
// Savings: 50-70% power reduction
```

### 4. Batch Processing
```c
// Accumulate data/logs
// Process in batches
// Minimize wake events
// Savings: 30-50% reduction in active time
```

## Configuration Guide

### Enabling Tickless Idle

In `FreeRTOSConfig.h`:
```c
#define configUSE_TICKLESS_IDLE                 1
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   2
```

### Implementing Port-Specific Functions

```c
// Must implement for your hardware:
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime);

// Optional hooks:
void vApplicationPreSleepProcessing(uint32_t *xExpectedIdleTime);
void vApplicationPostSleepProcessing(uint32_t xExpectedIdleTime);
```

### Wake Source Configuration

```c
// Configure which peripherals can wake system:
- RTC/Timer for scheduled wakes
- UART for communication
- GPIO for interrupts
- Network controller
- Sensor thresholds
```

## Power Consumption Analysis

### Current Draw Estimates

| Mode | CPU | Peripherals | Total | Savings |
|------|-----|------------|-------|---------|
| Active High | 100mA | 50mA | 150mA | 0% |
| Active Low | 40mA | 20mA | 60mA | 60% |
| Sleep | 2mA | 0.5mA | 2.5mA | 98% |
| Deep Sleep | 0.5mA | 0.1mA | 0.6mA | 99.6% |

### Battery Life Calculation

```
Battery Capacity: 2000mAh
Active Current: 100mA
Sleep Current: 2mA
Active Time: 20%
Sleep Time: 80%

Average Current = (100mA × 0.2) + (2mA × 0.8) = 21.6mA
Battery Life = 2000mAh / 21.6mA = 92.6 hours
```

Without tickless idle: 20 hours
With tickless idle: 92.6 hours
**Improvement: 4.6× battery life**

## Exercises

1. **Profile Testing**
   - Run each power profile for 1 minute
   - Compare actual vs target savings
   - Identify optimal settings

2. **Wake Latency Measurement**
   - Measure time from wake event to task execution
   - Optimize pre/post sleep processing
   - Find minimum viable sleep duration

3. **Battery Simulation**
   - Implement voltage decline curve
   - Test profile transitions
   - Verify low battery handling

4. **Custom Profile Creation**
   - Create application-specific profile
   - Balance performance vs power
   - Test with real workload

5. **Peripheral Optimization**
   - Add peripheral enable/disable
   - Measure power impact
   - Implement smart peripheral management

## Wind Turbine Application

Power management for remote wind turbine monitor:

### Operating Modes

1. **Generation Mode** (wind > 5 m/s)
   - High performance profile
   - Real-time monitoring
   - Network enabled
   - 1-second sensor updates

2. **Idle Mode** (wind < 5 m/s)
   - Ultra low power profile
   - Periodic health checks
   - Network disabled
   - 5-minute sensor updates

3. **Maintenance Mode**
   - Balanced profile
   - Remote access enabled
   - Diagnostic logging
   - 10-second updates

4. **Emergency Mode**
   - Override power saving
   - Continuous monitoring
   - All alerts enabled
   - Real-time response

### Expected Results

- **Normal Operation**: 60-70% power savings
- **Low Wind Periods**: 80-90% power savings
- **Battery Life**: 30 days → 6 months
- **Response Time**: < 100ms wake latency

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/08_power_save/power_save_example

# Observe:
# - Power statistics every 10 seconds
# - Profile changes based on battery
# - Wake source tracking
# - Sleep/wake cycles
```

## Troubleshooting

### Low Power Savings
- Check task activity patterns
- Increase sensor intervals
- Reduce wake sources
- Enable aggressive sleep

### Frequent Wakes
- Identify wake sources
- Batch operations
- Increase event timeouts
- Disable unnecessary interrupts

### Battery Drain
- Profile actual consumption
- Check peripheral states
- Verify sleep entry
- Optimize wake processing

## Key Takeaways

1. **Tickless idle can save 60-80% power** with proper configuration
2. **Profile selection** should be battery-aware
3. **Wake sources** must be carefully managed
4. **Batch processing** reduces active time
5. **Peripheral management** is critical
6. **Measure actual consumption** to validate
7. **Balance performance vs power** for application needs
8. **Test all power states** thoroughly
9. **Monitor battery health** continuously
10. **Plan for worst-case** power scenarios

## Integration with Final System

This power management capability optimizes the integrated Wind Turbine Monitor for long-term deployment:

### Direct Applications

1. **Adaptive Power Profiles**
   - High performance during active wind generation
   - Ultra-low power during calm periods
   - Battery-aware profile selection

2. **Smart Wake Management**
   - Timer-based for periodic monitoring
   - Event-driven for critical alerts
   - Minimized wake frequency

3. **Energy Harvesting Support**
   - Operates on turbine-generated power
   - Extends battery life to months
   - Self-sustaining operation

### Console Dashboard Preview

```
POWER MONITOR                         Profile: BALANCED
----------------------------------------------------------
CURRENT STATUS:
  Mode: SLEEP           Duration: 4.2s
  CPU: 32kHz           Power Draw: 2.5mA
  
POWER METRICS:
  Active Time: 20%      Sleep Time: 80%
  Average Current: 21mA Power Saved: 65%
  
BATTERY STATUS:
  Voltage: 3.8V         Level: 75%
  Runtime: 18.5 hours   Est. Life: 3.2 days
  
WAKE SOURCES (Last Hour):
  Timer: 234            Network: 12
  Sensor: 45            Alarm: 2
```

## Next Steps

Congratulations! You've completed all 8 FreeRTOS capabilities:
1. ✅ Task Scheduling
2. ✅ ISR Handling
3. ✅ Queue Communication
4. ✅ Mutex Protection
5. ✅ Event Groups
6. ✅ Memory Management
7. ✅ Stack Overflow Detection
8. ✅ Power Management

**Ready for Integration!** Next: Combine all capabilities into the complete Wind Turbine Monitoring System with threshold-based anomaly detection and real-time console dashboard!