# Example 03: Queue-Based Producer-Consumer Pattern

## Overview

This example demonstrates a complete producer-consumer system using FreeRTOS queues. It simulates the data flow in our wind turbine predictive maintenance system, with multiple sensors producing data at different rates and multiple consumers processing, logging, and transmitting that data.

## System Architecture

```
Producers                 Queues                    Consumers
---------                 ------                    ---------
Fast (100Hz) ──┐                                ┌──> Processing
               ├──> Sensor Queue ────────────────┤    (Anomaly Detection)
Medium (10Hz) ─┤    [20 items]                  └──> │
               │                                      ↓
Burst (Event) ─┘                                 Processed Queue
                                                  [10 items]
                                                      │
                                                      ├──> Logging
                    Alert Queue                      │    (Data Storage)
                    [5 items]                        │
                         ↑                           │
                         └───────────────────────────┘
                         │
                         └──> Network
                              (Alert Transmission)
```

## Components

### Producers

1. **Fast Producer (100Hz)**
   - Simulates vibration sensor
   - Generates data every 10ms
   - Values: 50-60 range
   - High priority when value > 58

2. **Medium Producer (10Hz)**
   - Simulates temperature sensor
   - Generates data every 100ms
   - Values: 20-25 range
   - High priority when value > 24

3. **Burst Producer (Event-based)**
   - Simulates event-triggered sensor
   - Generates 3-7 items in burst
   - Random intervals (0.5-2.5 seconds)
   - Always high priority

### Consumers

1. **Processing Consumer**
   - Priority 5 (High)
   - Performs anomaly detection
   - Calculates anomaly scores
   - Generates alerts for anomalies
   - Updates baseline dynamically

2. **Logging Consumer**
   - Priority 1 (Low)
   - Simulates data storage
   - Processes at slower rate
   - Logs every 10th item

3. **Network Consumer**
   - Priority 6 (Highest)
   - Transmits critical alerts
   - Simulates network latency
   - Handles only alert data

### Monitoring

1. **Queue Monitor**
   - Uses queue sets
   - Monitors multiple queues simultaneously
   - Tracks queue depths
   - Identifies high-priority data

2. **Statistics Task**
   - Reports every 5 seconds
   - Tracks production/consumption rates
   - Calculates efficiency metrics
   - Monitors dropped data

## Key Features Demonstrated

### 1. Queue Sizing
```c
#define SENSOR_QUEUE_SIZE    20  // Based on 100Hz rate and processing time
#define PROCESSED_QUEUE_SIZE 10  // Lower rate after processing
#define ALERT_QUEUE_SIZE     5   // Only critical alerts
```

### 2. Overflow Handling
```c
if (xQueueSend(xQueue, &data, timeout) != pdTRUE) {
    // Queue full - track dropped data
    g_stats.dropped++;
    // Could implement different strategies:
    // - Drop oldest (xQueueOverwrite)
    // - Drop newest (current)
    // - Block and wait
}
```

### 3. Queue Sets
```c
// Monitor multiple queues with single blocking call
xQueueSet = xQueueCreateSet(totalQueueSizes);
xQueueAddToSet(xSensorQueue, xQueueSet);
xQueueAddToSet(xProcessedQueue, xQueueSet);

// Wait for any queue to have data
xActivatedMember = xQueueSelectFromSet(xQueueSet, timeout);
```

### 4. Performance Metrics
- Production rate per producer
- Consumption rate per consumer
- Average latency (producer to consumer)
- Queue utilization
- Drop rate
- Alert generation rate

## Expected Output

```
===========================================
Example 03: Queue-Based Producer-Consumer
Multiple producers and consumers
===========================================

[FAST PRODUCER] Started (100Hz vibration sensor)
[MEDIUM PRODUCER] Started (10Hz temperature sensor)
[BURST PRODUCER] Started (event-based sensor)
[PROCESSING CONSUMER] Started (anomaly detection)
[LOGGING CONSUMER] Started (data logger)
[NETWORK CONSUMER] Started (alert transmitter)

[BURST PRODUCER] Generating burst of 5 items
[PROCESSING] ALERT! Sequence 20001, Score 45.2%, Level 2
[NETWORK] Transmitting alert: Level 2, Score 45.2%
[MONITOR] High priority data in sensor queue!

[MONITOR] Queues: Sensor=12/20, Processed=3/10, Alert=1/5

========================================
QUEUE SYSTEM STATISTICS
========================================
Producers:
  Fast (100Hz):   500 items
  Medium (10Hz):  50 items
  Burst:          15 items
  Total Produced: 565 items
  Dropped:        2 items (0.4%)

Consumers:
  Processing:     563 items
  Logging:        450 items
  Network:        8 items
  Total Consumed: 1021 items

Performance:
  Avg Latency:    2.3 ticks
  Max Queue Use:  18/20 items
  Alerts:         8 generated
  Efficiency:     99.6%
========================================
```

## Learning Points

### 1. Queue Sizing Formula
```
Queue Size = Producer Rate × Max Consumer Latency × Safety Factor

Example for Fast Producer:
- Rate: 100Hz (10ms period)
- Consumer latency: 50ms worst case
- Items during latency: 100Hz × 0.05s = 5 items
- Safety factor: 2x for bursts
- Queue size: 5 × 2 = 10 minimum (we use 20 for extra margin)
```

### 2. Priority Handling
- Processing consumer has high priority (5)
- Ensures sensor data is processed quickly
- Network consumer has highest priority (6)
- Critical alerts transmitted immediately

### 3. Data Flow Patterns
- **Direct**: Sensor → Processing
- **Pipeline**: Sensor → Processing → Logging
- **Branch**: Processing → Both Logging and Alerts
- **Priority Path**: Alerts → Network (bypass normal flow)

## Exercises

1. **Adjust Queue Sizes**
   - Reduce SENSOR_QUEUE_SIZE to 5
   - Observe increased drop rate
   - Find optimal size for zero drops

2. **Add Back-Pressure**
   - Make producer block when queue is 80% full
   - Implement adaptive rate control

3. **Implement Queue Overflow Strategies**
   - Try xQueueOverwrite for "drop oldest"
   - Implement circular buffer behavior

4. **Add Priority Queues**
   - Create separate high/low priority queues
   - Route data based on priority level

5. **Measure Queue Latency**
   - Add timestamps to track end-to-end latency
   - Find bottlenecks in the pipeline

## Wind Turbine Application

This pattern directly applies to our wind turbine system:

1. **Vibration Monitoring**
   - 100Hz accelerometer data
   - Real-time anomaly detection
   - Critical vibration alerts

2. **Temperature Tracking**
   - 10Hz temperature sensors
   - Bearing overheat detection
   - Predictive maintenance triggers

3. **Event Handling**
   - Emergency stop events
   - Maintenance requests
   - System alerts

4. **Data Pipeline**
   - Sensor collection → Processing → Storage
   - Anomaly detection → Alert generation
   - Critical events → Immediate response

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/03_producer_consumer/producer_consumer_example

# Observe:
# - Queue utilization
# - Drop rates
# - Alert generation
# - Statistics reports
```

## Integration with Final System

This queue communication capability is crucial for the integrated Wind Turbine Monitor:

### Direct Applications

1. **Sensor Data Pipeline**
   - Multiple sensor tasks produce data at different rates
   - Central processing task consumes and analyzes
   - Threshold-based anomaly detection on queue data

2. **Alert Notification System**
   - Critical alerts use high priority queue
   - Normal updates use standard queue
   - Network task consumes and transmits alerts

3. **Data Buffering**
   - Handles burst sensor readings
   - Prevents data loss during network delays
   - Enables batch processing for efficiency

### Console Dashboard Preview

```
QUEUE MONITOR                          Active: 3/5 Queues
----------------------------------------------------------
SENSOR QUEUE:
  Size: 50/100 msgs  [##########----------] 50%
  Rate: 45 msg/s     Producer: SensorTask
  
ALERT QUEUE:
  Size: 2/10 msgs    [##------------------] 20% 
  Priority: HIGH     Last: VIBRATION_WARNING
  
NETWORK QUEUE:
  Size: 15/50 msgs   [######--------------] 30%
  Pending: 3 pkts    Status: TRANSMITTING
```

## Next Steps

After mastering queue communication, proceed to:
- Example 04: Mutex for Shared Resources
- Then continue through remaining capabilities
- Finally integrate all 8 capabilities into complete system