/**
 * Anomaly Detection Task - Threshold-based detection with ML-ready architecture
 * Priority: 3 (Medium)
 * Frequency: 5Hz
 */

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "../common/system_state.h"

// Detection parameters
#define ANOMALY_CHECK_RATE_MS   200  // 5Hz
#define HISTORY_SIZE           100
#define BASELINE_WINDOW        20

// External references
extern SystemState_t g_system_state;
extern ThresholdConfig_t g_thresholds;
extern QueueHandle_t xSensorDataQueue;    // Capability 3: Receive sensor data
extern QueueHandle_t xAnomalyAlertQueue;  // Capability 3: Send alerts
extern SemaphoreHandle_t xSystemStateMutex;  // Capability 4: Mutex protection
extern SemaphoreHandle_t xThresholdsMutex;   // Capability 4: Mutex protection
extern EventGroupHandle_t xSystemReadyEvents;  // Capability 5: Event groups

// Event bits (defined in main.c)
#define ANOMALY_READY_BIT       (1 << 2)  // 0x04 - AnomalyTask baseline ready

// Detection state
typedef struct {
    float vibration_history[HISTORY_SIZE];
    float temperature_history[HISTORY_SIZE];
    float rpm_history[HISTORY_SIZE];
    uint32_t history_index;
    
    float vibration_baseline;
    float temperature_baseline;
    float rpm_baseline;
    
    float vibration_stddev;
    float temperature_stddev;
    float rpm_stddev;
} DetectionState_t;

static DetectionState_t detection_state = {0};

// Calculate mean of array
static float calculate_mean(float* data, uint32_t size) {
    float sum = 0;
    for (uint32_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum / size;
}

// Calculate standard deviation
static float calculate_stddev(float* data, uint32_t size, float mean) {
    float sum_sq = 0;
    for (uint32_t i = 0; i < size; i++) {
        float diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / size);
}

// Update baselines using moving average
static void update_baselines(void) {
    uint32_t start_idx = 0;
    uint32_t count = BASELINE_WINDOW;
    
    if (detection_state.history_index < BASELINE_WINDOW) {
        count = detection_state.history_index;
    } else {
        start_idx = detection_state.history_index - BASELINE_WINDOW;
    }
    
    if (count > 0) {
        // Calculate baselines
        detection_state.vibration_baseline = calculate_mean(
            &detection_state.vibration_history[start_idx], count);
        detection_state.temperature_baseline = calculate_mean(
            &detection_state.temperature_history[start_idx], count);
        detection_state.rpm_baseline = calculate_mean(
            &detection_state.rpm_history[start_idx], count);
        
        // Calculate standard deviations
        detection_state.vibration_stddev = calculate_stddev(
            &detection_state.vibration_history[start_idx], count, 
            detection_state.vibration_baseline);
        detection_state.temperature_stddev = calculate_stddev(
            &detection_state.temperature_history[start_idx], count,
            detection_state.temperature_baseline);
        detection_state.rpm_stddev = calculate_stddev(
            &detection_state.rpm_history[start_idx], count,
            detection_state.rpm_baseline);
    }
}

// Detect anomalies using threshold method
static void detect_anomalies(void) {
    // Get current readings (protected)
    float vib = 0, temp = 0, rpm = 0;
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        vib = g_system_state.sensors.vibration;
        temp = g_system_state.sensors.temperature;
        rpm = g_system_state.sensors.rpm;
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
    
    // Store in history
    uint32_t idx = detection_state.history_index % HISTORY_SIZE;
    detection_state.vibration_history[idx] = vib;
    detection_state.temperature_history[idx] = temp;
    detection_state.rpm_history[idx] = rpm;
    detection_state.history_index++;
    
    // Update baselines
    update_baselines();
    
    // Detect anomalies (3-sigma rule) - protected write
    bool vib_anomaly = false, temp_anomaly = false, rpm_anomaly = false;
    uint32_t anomaly_count = 0;
    
    // Get threshold values (protected)
    float vib_warning = 5.0, temp_warning = 70.0, rpm_min = 10.0, rpm_max = 30.0;
    if (xSemaphoreTake(xThresholdsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.threshold_mutex_takes++;
        vib_warning = g_thresholds.vibration_warning;
        temp_warning = g_thresholds.temperature_warning;
        rpm_min = g_thresholds.rpm_min;
        rpm_max = g_thresholds.rpm_max;
        g_system_state.mutex_stats.threshold_mutex_gives++;
        xSemaphoreGive(xThresholdsMutex);
    } else {
        g_system_state.mutex_stats.threshold_mutex_timeouts++;
    }
    
    if (detection_state.history_index > BASELINE_WINDOW) {
        // Vibration anomaly
        float vib_deviation = fabs(vib - detection_state.vibration_baseline);
        if (vib_deviation > 3.0 * detection_state.vibration_stddev ||
            vib > vib_warning) {
            vib_anomaly = true;
            anomaly_count++;
        }
        
        // Temperature anomaly
        float temp_deviation = fabs(temp - detection_state.temperature_baseline);
        if (temp_deviation > 3.0 * detection_state.temperature_stddev ||
            temp > temp_warning) {
            temp_anomaly = true;
            anomaly_count++;
        }
        
        // RPM anomaly
        float rpm_deviation = fabs(rpm - detection_state.rpm_baseline);
        if (rpm_deviation > 3.0 * detection_state.rpm_stddev ||
            rpm < rpm_min || rpm > rpm_max) {
            rpm_anomaly = true;
            anomaly_count++;
        }
    }
    
    // Calculate health score (0-100%)
    float health = 100.0;
    
    // Reduce health based on deviations
    if (detection_state.vibration_stddev > 0) {
        float vib_score = fabs(vib - detection_state.vibration_baseline) / 
                         (detection_state.vibration_stddev * 3.0);
        health -= fmin(vib_score * 20.0, 30.0);  // Max 30% reduction
    }
    
    if (detection_state.temperature_stddev > 0) {
        float temp_score = fabs(temp - detection_state.temperature_baseline) / 
                          (detection_state.temperature_stddev * 3.0);
        health -= fmin(temp_score * 15.0, 25.0);  // Max 25% reduction
    }
    
    if (detection_state.rpm_stddev > 0) {
        float rpm_score = fabs(rpm - detection_state.rpm_baseline) / 
                         (detection_state.rpm_stddev * 3.0);
        health -= fmin(rpm_score * 15.0, 25.0);  // Max 25% reduction
    }
    
    // Check emergency stop status
    bool emergency = false;
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        emergency = g_system_state.emergency_stop;
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
    
    if (emergency) {
        health = 0;
    }
    
    // Update anomaly results in protected section
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        g_system_state.anomalies.vibration_anomaly = vib_anomaly;
        g_system_state.anomalies.temperature_anomaly = temp_anomaly;
        g_system_state.anomalies.rpm_anomaly = rpm_anomaly;
        if (anomaly_count > 0) {
            g_system_state.anomalies.anomaly_count += anomaly_count;
        }
        g_system_state.anomalies.health_score = fmax(health, 0.0);
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
}

void vAnomalyTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(ANOMALY_CHECK_RATE_MS);
    
    uint32_t cycle_count = 0;
    bool anomaly_ready = false;
    
    while (1) {
        // Wait for the next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        cycle_count++;
        
        // Process queue items with balanced consumption (Capability 3)
        SensorData_t sensor_data;
        int items_processed = 0;
        
        // Process 1-2 items per cycle to show dynamic queue behavior
        // 5Hz * 1.5 avg = 7.5Hz consumption vs 10Hz production = queue builds up
        int items_to_consume = (cycle_count % 2 == 0) ? 1 : 2;  // Alternate between 1 and 2
        
        while (items_processed < items_to_consume &&
               xQueueReceive(xSensorDataQueue, &sensor_data, 0) == pdTRUE) {
            items_processed++;
            // Keep the latest data for processing (protected)
            if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_system_state.mutex_stats.system_mutex_takes++;
                g_system_state.sensors = sensor_data;
                g_system_state.mutex_stats.system_mutex_gives++;
                xSemaphoreGive(xSystemStateMutex);
            } else {
                g_system_state.mutex_stats.system_mutex_timeouts++;
            }
        }
        
        // If we got any data, run anomaly detection
        if (items_processed > 0) {
            // Perform anomaly detection on latest data
            detect_anomalies();
            
            // Check if anomaly detection is ready (after baseline window filled) - Capability 5
            if (!anomaly_ready && detection_state.history_index >= BASELINE_WINDOW) {
                anomaly_ready = true;
                // Set the anomaly ready bit in event group
                xEventGroupSetBits(xSystemReadyEvents, ANOMALY_READY_BIT);
                
                // Update statistics (protected)
                if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_system_state.mutex_stats.system_mutex_takes++;
                    g_system_state.event_group_stats.bits_set_count++;
                    g_system_state.event_group_stats.current_event_bits |= ANOMALY_READY_BIT;
                    g_system_state.mutex_stats.system_mutex_gives++;
                    xSemaphoreGive(xSystemStateMutex);
                } else {
                    g_system_state.mutex_stats.system_mutex_timeouts++;
                }
            }
            
            // Send anomaly alerts more frequently to show queue usage
            // Send every 2nd cycle when anomaly detected (2.5Hz)
            bool send_alert = false;
            AnomalyAlert_t alert;
            
            if (cycle_count % 2 == 0) {
                // Check anomaly status (protected)
                if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_system_state.mutex_stats.system_mutex_takes++;
                    if (g_system_state.anomalies.vibration_anomaly || 
                        g_system_state.anomalies.temperature_anomaly) {
                        send_alert = true;
                        alert.severity = g_system_state.anomalies.vibration_anomaly ? 8.0 : 5.0;
                        alert.type = g_system_state.anomalies.vibration_anomaly ? 0 : 1;
                        alert.timestamp = xTaskGetTickCount();
                    }
                    g_system_state.mutex_stats.system_mutex_gives++;
                    xSemaphoreGive(xSystemStateMutex);
                } else {
                    g_system_state.mutex_stats.system_mutex_timeouts++;
                }
            }
            
            if (send_alert) {
                
                // Send alert (non-blocking)
                xQueueSend(xAnomalyAlertQueue, &alert, 0);
            }
        } else {
            // No data in queue - still run detection with current state
            detect_anomalies();
        }
        
        // Occasionally yield to demonstrate scheduling
        if (cycle_count % 5 == 0) {
            taskYIELD();
        }
        
        // Update context switch count
        // g_system_state.context_switch_count++; // Now tracked in update_task_stats()
    }
}
