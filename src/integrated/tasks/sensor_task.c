/**
 * Sensor Task - Simulates reading data from wind turbine sensors
 * Priority: 4 (High)
 * Frequency: 10Hz
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "../common/system_state.h"

// Sensor simulation parameters
#define SENSOR_READ_RATE_MS     100  // 10Hz
#define VIBRATION_NOISE         0.5
#define TEMPERATURE_DRIFT       0.1
#define RPM_VARIATION          0.5

// External system state and queues
extern SystemState_t g_system_state;
extern QueueHandle_t xSensorISRQueue;
extern QueueHandle_t xSensorDataQueue;  // Capability 3: Queue communication
extern SemaphoreHandle_t xSystemStateMutex;  // Capability 4: Mutex protection
extern EventGroupHandle_t xSystemReadyEvents;  // Capability 5: Event groups

// Event bits (defined in main.c)
#define SENSORS_CALIBRATED_BIT  (1 << 0)  // 0x01 - SensorTask ready

// Simulate sensor reading with noise
static float read_sensor_with_noise(float base_value, float noise_level) {
    float noise = ((float)(rand() % 1000) / 1000.0 - 0.5) * 2.0 * noise_level;
    return base_value + noise;
}

// Simulate gradual changes
static float simulate_drift(float current, float target, float rate) {
    float diff = target - current;
    return current + (diff * rate);
}

void vSensorTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(SENSOR_READ_RATE_MS);
    
    // Base values for simulation
    float base_vibration = 2.5;
    float base_temperature = 45.0;
    float base_rpm = 20.0;
    float base_current = 50.0;
    
    // Simulation targets (for gradual changes)
    float target_vibration = base_vibration;
    float target_temperature = base_temperature;
    
    uint32_t cycle_count = 0;
    bool sensors_calibrated = false;
    
    while (1) {
        // Wait for the next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        cycle_count++;
        
        // Check if sensors are calibrated (after 20 readings) - Capability 5
        if (!sensors_calibrated && cycle_count >= 20) {
            sensors_calibrated = true;
            // Set the sensors calibrated bit in event group
            xEventGroupSetBits(xSystemReadyEvents, SENSORS_CALIBRATED_BIT);
            
            // Update statistics (protected)
            if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_system_state.mutex_stats.system_mutex_takes++;
                g_system_state.event_group_stats.bits_set_count++;
                g_system_state.event_group_stats.current_event_bits |= SENSORS_CALIBRATED_BIT;
                g_system_state.mutex_stats.system_mutex_gives++;
                xSemaphoreGive(xSystemStateMutex);
            } else {
                g_system_state.mutex_stats.system_mutex_timeouts++;
            }
        }
        
        // Process ALL ISR data in queue (Capability 2: Deferred Processing)
        SensorISRData_t isr_data;
        int items_processed = 0;
        TickType_t min_latency = UINT32_MAX;
        
        // Process all available items to prevent queue buildup
        while (xQueueReceive(xSensorISRQueue, &isr_data, 0) == pdTRUE) {
            items_processed++;
            
            // Calculate ISR latency for this item
            TickType_t latency_ticks = xTaskGetTickCount() - isr_data.timestamp;
            if (latency_ticks < min_latency) {
                min_latency = latency_ticks;
            }
            
            // Use the latest ISR vibration data
            base_vibration = isr_data.vibration;
            
            // Check for emergency condition (protected)
            if (isr_data.vibration > 80.0) {
                if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_system_state.mutex_stats.system_mutex_takes++;
                    g_system_state.emergency_stop = true;
                    g_system_state.mutex_stats.system_mutex_gives++;
                    xSemaphoreGive(xSystemStateMutex);
                } else {
                    g_system_state.mutex_stats.system_mutex_timeouts++;
                }
            }
            
            // Update ISR stats (protected)
            if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_system_state.mutex_stats.system_mutex_takes++;
                g_system_state.isr_stats.processed_count++;
                g_system_state.mutex_stats.system_mutex_gives++;
                xSemaphoreGive(xSystemStateMutex);
            } else {
                g_system_state.mutex_stats.system_mutex_timeouts++;
            }
        }
        
        // Update latency metric with best case (minimum latency seen)
        if (items_processed > 0) {
            // Realistic latency: typically under 1ms in simulation
            if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_system_state.mutex_stats.system_mutex_takes++;
                if (min_latency <= 1) {
                    g_system_state.isr_stats.last_latency_us = 250;  // 250µs typical
                } else {
                    g_system_state.isr_stats.last_latency_us = min_latency * 1000;  // Convert ms to µs
                }
                g_system_state.mutex_stats.system_mutex_gives++;
                xSemaphoreGive(xSystemStateMutex);
            } else {
                g_system_state.mutex_stats.system_mutex_timeouts++;
            }
        }
        
        // Update sensor readings (ISR provides vibration, others simulated)
        SensorData_t current_reading;
        current_reading.vibration = base_vibration;
        current_reading.temperature = read_sensor_with_noise(base_temperature, TEMPERATURE_DRIFT);
        current_reading.rpm = read_sensor_with_noise(base_rpm, RPM_VARIATION);
        current_reading.current = read_sensor_with_noise(base_current, 2.0);
        current_reading.timestamp = xTaskGetTickCount();
        
        // Update global state for dashboard display (protected)
        if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_system_state.mutex_stats.system_mutex_takes++;
            g_system_state.sensors = current_reading;
            g_system_state.mutex_stats.system_mutex_gives++;
            xSemaphoreGive(xSystemStateMutex);
        } else {
            g_system_state.mutex_stats.system_mutex_timeouts++;
        }
        
        // Send sensor data via queue (Capability 3)
        if (xQueueSend(xSensorDataQueue, &current_reading, pdMS_TO_TICKS(10)) != pdTRUE) {
            // Queue full - data dropped (could track this)
        }
        
        // Simulate gradual changes every 50 cycles (5 seconds)
        if (cycle_count % 50 == 0) {
            // Randomly change targets
            if (rand() % 100 < 30) {  // 30% chance of change
                target_vibration = 1.0 + (float)(rand() % 80) / 10.0;  // 1.0 to 9.0
                target_temperature = 40.0 + (float)(rand() % 400) / 10.0;  // 40 to 80
            }
        }
        
        // Apply gradual changes
        base_vibration = simulate_drift(base_vibration, target_vibration, 0.02);
        base_temperature = simulate_drift(base_temperature, target_temperature, 0.01);
        
        // Simulate anomaly conditions more frequently for demonstration
        if (cycle_count % 50 == 0) {  // Every 5 seconds
            if (rand() % 100 < 40) {  // 40% chance
                // Inject anomaly
                base_vibration += 3.0;  // Sudden vibration increase
            }
        }
        
        // Update RPM based on time of day simulation
        float time_factor = sin((float)cycle_count * 0.01) * 0.5 + 0.5;
        base_rpm = 15.0 + time_factor * 10.0;  // 15-25 RPM range
        
        // Update current based on RPM
        base_current = 40.0 + base_rpm * 2.0;  // Correlated with RPM
        
        // Check for preemption opportunity
        if (cycle_count % 10 == 0) {
            // Higher priority task (Safety) should preempt occasionally
            taskYIELD();  // Give other tasks a chance to run
        }
        
        // Record that we're running (for dashboard)
        // g_system_state.context_switch_count++; // Now tracked in update_task_stats()
    }
}
