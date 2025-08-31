/**
 * Safety Task - Critical safety monitoring with highest priority
 * Priority: 6 (Highest)
 * Frequency: 50Hz for critical checks
 */

#include <stdio.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "../common/system_state.h"

// Safety parameters
#define SAFETY_CHECK_RATE_MS    20   // 50Hz for critical monitoring
#define EMERGENCY_STOP_DURATION 5000 // 5 seconds

// External references
extern SystemState_t g_system_state;
extern ThresholdConfig_t g_thresholds;
extern SemaphoreHandle_t xSystemStateMutex;
extern SemaphoreHandle_t xThresholdsMutex;
extern EventGroupHandle_t xSystemReadyEvents;  // Capability 5: Event groups
extern void record_preemption(const char* preemptor, const char* preempted, const char* reason);

// Event bits (defined in main.c)
#define SENSORS_CALIBRATED_BIT  (1 << 0)  // 0x01 - SensorTask ready
#define NETWORK_CONNECTED_BIT   (1 << 1)  // 0x02 - NetworkTask connected
#define ANOMALY_READY_BIT       (1 << 2)  // 0x04 - AnomalyTask baseline ready
#define ALL_SYSTEMS_READY       (SENSORS_CALIBRATED_BIT | NETWORK_CONNECTED_BIT | ANOMALY_READY_BIT)

// Safety state
typedef struct {
    bool vibration_alarm;
    bool temperature_alarm;
    bool rpm_alarm;
    bool current_alarm;
    TickType_t emergency_stop_time;
    uint32_t alarm_count;
} SafetyState_t;

static SafetyState_t safety_state = {0};

// Check for critical conditions
static bool check_critical_conditions(void) {
    bool critical = false;
    
    // Get sensor values and thresholds (protected)
    float vib = 0, temp = 0, rpm = 0, current = 0;
    float vib_crit = 10.0, temp_crit = 85.0, rpm_min = 10.0, rpm_max = 30.0, current_max = 100.0;
    
    // Get sensor data (protected)
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        vib = g_system_state.sensors.vibration;
        temp = g_system_state.sensors.temperature;
        rpm = g_system_state.sensors.rpm;
        current = g_system_state.sensors.current;
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
    
    // Get threshold values (protected)
    if (xSemaphoreTake(xThresholdsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.threshold_mutex_takes++;
        vib_crit = g_thresholds.vibration_critical;
        temp_crit = g_thresholds.temperature_critical;
        rpm_min = g_thresholds.rpm_min;
        rpm_max = g_thresholds.rpm_max;
        current_max = g_thresholds.current_max;
        g_system_state.mutex_stats.threshold_mutex_gives++;
        xSemaphoreGive(xThresholdsMutex);
    } else {
        g_system_state.mutex_stats.threshold_mutex_timeouts++;
    }
    
    // Check vibration
    if (vib > vib_crit) {
        if (!safety_state.vibration_alarm) {
            safety_state.vibration_alarm = true;
            safety_state.alarm_count++;
            critical = true;
        }
    } else {
        safety_state.vibration_alarm = false;
    }
    
    // Check temperature
    if (temp > temp_crit) {
        if (!safety_state.temperature_alarm) {
            safety_state.temperature_alarm = true;
            safety_state.alarm_count++;
            critical = true;
        }
    } else {
        safety_state.temperature_alarm = false;
    }
    
    // Check RPM
    if (rpm < rpm_min || rpm > rpm_max) {
        if (!safety_state.rpm_alarm) {
            safety_state.rpm_alarm = true;
            safety_state.alarm_count++;
            critical = true;
        }
    } else {
        safety_state.rpm_alarm = false;
    }
    
    // Check current
    if (current > current_max) {
        if (!safety_state.current_alarm) {
            safety_state.current_alarm = true;
            safety_state.alarm_count++;
            critical = true;
        }
    } else {
        safety_state.current_alarm = false;
    }
    
    return critical;
}

// Trigger emergency stop
static void trigger_emergency_stop(void) {
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        g_system_state.emergency_stop = true;
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
    safety_state.emergency_stop_time = xTaskGetTickCount();
    
    // Record this critical event
    record_preemption("SafetyTask", "ALL", "EMERGENCY");
}

// Clear emergency stop after timeout
static void check_emergency_clear(void) {
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
        TickType_t elapsed = xTaskGetTickCount() - safety_state.emergency_stop_time;
        if (elapsed > pdMS_TO_TICKS(EMERGENCY_STOP_DURATION)) {
            // Clear emergency stop if conditions are safe
            if (!check_critical_conditions()) {
                if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_system_state.mutex_stats.system_mutex_takes++;
                    g_system_state.emergency_stop = false;
                    g_system_state.mutex_stats.system_mutex_gives++;
                    xSemaphoreGive(xSystemStateMutex);
                } else {
                    g_system_state.mutex_stats.system_mutex_timeouts++;
                }
            }
        }
    }
}

void vSafetyTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(SAFETY_CHECK_RATE_MS);
    
    uint32_t cycle_count = 0;
    uint32_t preemption_demo_count = 0;
    bool system_ready = false;
    
    // Wait for all systems to be ready before starting safety monitoring (Capability 5)
    printf("[SAFETY] Waiting for all systems to be ready...\n");
    EventBits_t ready_bits = xEventGroupWaitBits(
        xSystemReadyEvents,
        ALL_SYSTEMS_READY,
        pdFALSE,  // Don't clear bits
        pdTRUE,   // Wait for ALL bits
        portMAX_DELAY  // Wait indefinitely
    );
    
    // Update event group statistics (protected)
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        g_system_state.event_group_stats.wait_operations++;
        g_system_state.event_group_stats.system_ready_time = xTaskGetTickCount();
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
    
    if ((ready_bits & ALL_SYSTEMS_READY) == ALL_SYSTEMS_READY) {
        system_ready = true;
        printf("[SAFETY] All systems ready! Starting safety monitoring...\n");
    }
    
    while (1) {
        // Wait for the next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        cycle_count++;
        
        // Perform critical safety checks
        bool critical = check_critical_conditions();
        
        // Handle critical conditions
        if (critical) {
            // Multiple alarms = emergency stop
            uint32_t active_alarms = 0;
            if (safety_state.vibration_alarm) active_alarms++;
            if (safety_state.temperature_alarm) active_alarms++;
            if (safety_state.rpm_alarm) active_alarms++;
            if (safety_state.current_alarm) active_alarms++;
            
            if (active_alarms >= 2) {
                trigger_emergency_stop();
            }
        }
        
        // Check if emergency stop can be cleared
        check_emergency_clear();
        
        // Demonstrate preemption every 2 seconds
        if (cycle_count % 100 == 0) {
            preemption_demo_count++;
            
            // This high-priority task will preempt lower priority tasks
            // Record the preemption for dashboard display
            TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
            TaskStatus_t task_status;
            vTaskGetInfo(current_task, &task_status, pdFALSE, eInvalid);
            
            // Simulate detecting which task we preempted
            if (preemption_demo_count % 4 == 0) {
                record_preemption("SafetyTask", "SensorTask", "Priority");
            } else if (preemption_demo_count % 4 == 1) {
                record_preemption("SafetyTask", "AnomalyTask", "Priority");
            } else if (preemption_demo_count % 4 == 2) {
                record_preemption("SafetyTask", "NetworkTask", "Priority");
            } else {
                record_preemption("SafetyTask", "DashboardTask", "Priority");
            }
        }
        
        // Update system state
        // g_system_state.context_switch_count++; // Now tracked in update_task_stats()
        
        // Brief computational work to simulate processing
        volatile uint32_t dummy = 0;
        for (uint32_t i = 0; i < 1000; i++) {
            dummy += i;
        }
    }
}
