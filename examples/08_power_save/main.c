/**
 * Example 08: Power Management with Tickless Idle
 * 
 * Demonstrates:
 * - Tickless idle configuration
 * - Dynamic power profiles
 * - Wake source management
 * - Power consumption monitoring
 * - Battery-aware operation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

// Task configurations
#define SENSOR_TASK_STACK_SIZE    (1024)
#define NETWORK_TASK_STACK_SIZE   (1024)
#define LOGGER_TASK_STACK_SIZE    (1024)
#define MONITOR_TASK_STACK_SIZE   (1024)
#define ALARM_TASK_STACK_SIZE     (512)

// Power states
typedef enum {
    POWER_STATE_RUN,
    POWER_STATE_IDLE,
    POWER_STATE_SLEEP,
    POWER_STATE_DEEP_SLEEP
} PowerState_t;

// Wake sources
typedef enum {
    WAKE_SOURCE_TIMER,
    WAKE_SOURCE_NETWORK,
    WAKE_SOURCE_SENSOR,
    WAKE_SOURCE_ALARM,
    WAKE_SOURCE_UNKNOWN
} WakeSource_t;

// Power profile
typedef struct {
    const char *name;
    uint32_t sensorInterval;
    bool networkEnabled;
    bool aggressiveSleep;
    float targetSaving;
} PowerProfile_t;

// Power statistics
typedef struct {
    uint32_t totalTicks;
    uint32_t idleTicks;
    uint32_t sleepCount;
    uint32_t wakeCount[5];
    float powerSavingPercent;
    PowerState_t currentState;
    uint32_t lastSleepDuration;
    uint32_t longestSleep;
} PowerStats_t;

// Simulated battery info
typedef struct {
    uint16_t voltage_mV;
    uint8_t percentage;
    bool isCharging;
} BatteryInfo_t;

// Global variables
static PowerStats_t powerStats = {0};
static PowerProfile_t currentProfile;
static EventGroupHandle_t xSystemEvents;
static SemaphoreHandle_t xPowerMutex;
static volatile bool simulateActivity = false;
static volatile WakeSource_t lastWakeSource = WAKE_SOURCE_UNKNOWN;

// Event bits
#define SENSOR_DATA_READY_BIT    (1 << 0)
#define NETWORK_PACKET_BIT       (1 << 1)
#define ALARM_TRIGGERED_BIT      (1 << 2)
#define LOW_BATTERY_BIT          (1 << 3)

// Power profiles
PowerProfile_t powerProfiles[] = {
    {"High Performance", 1000, true, false, 0.0},
    {"Balanced", 5000, true, false, 30.0},
    {"Power Saver", 30000, false, true, 60.0},
    {"Ultra Low Power", 60000, false, true, 80.0}
};

// Simulated tickless idle (for demonstration)
static TickType_t totalIdleTime = 0;
static TickType_t lastIdleEntry = 0;

// Hook functions for tickless idle
void vApplicationIdleHook(void) {
    // Track idle time
    if (lastIdleEntry == 0) {
        lastIdleEntry = xTaskGetTickCount();
    }
}

// Simulate sleep processing
void simulatePreSleepProcessing(TickType_t xExpectedIdleTime) {
    xSemaphoreTake(xPowerMutex, portMAX_DELAY);
    
    printf("[POWER] Entering sleep for %lu ms\n", 
           xExpectedIdleTime * portTICK_PERIOD_MS);
    
    // Would disable peripherals here
    powerStats.currentState = POWER_STATE_SLEEP;
    powerStats.sleepCount++;
    
    xSemaphoreGive(xPowerMutex);
}

void simulatePostSleepProcessing(TickType_t xSleptTime) {
    xSemaphoreTake(xPowerMutex, portMAX_DELAY);
    
    printf("[POWER] Woke after %lu ms (source: %d)\n", 
           xSleptTime * portTICK_PERIOD_MS, lastWakeSource);
    
    // Would re-enable peripherals here
    powerStats.currentState = POWER_STATE_RUN;
    powerStats.lastSleepDuration = xSleptTime;
    
    if (xSleptTime > powerStats.longestSleep) {
        powerStats.longestSleep = xSleptTime;
    }
    
    powerStats.wakeCount[lastWakeSource]++;
    powerStats.idleTicks += xSleptTime;
    
    xSemaphoreGive(xPowerMutex);
}

// Simulated battery monitoring
BatteryInfo_t getBatteryInfo(void) {
    static uint16_t simVoltage = 4200;  // Start full
    BatteryInfo_t info;
    
    // Simulate discharge
    if (!info.isCharging && (rand() % 100) < 10) {
        simVoltage -= 10;
        if (simVoltage < 3000) simVoltage = 3000;
    }
    
    info.voltage_mV = simVoltage;
    info.isCharging = false;
    
    // Calculate percentage (3.0V = 0%, 4.2V = 100%)
    if (info.voltage_mV >= 4200) {
        info.percentage = 100;
    } else if (info.voltage_mV <= 3000) {
        info.percentage = 0;
    } else {
        info.percentage = ((info.voltage_mV - 3000) * 100) / 1200;
    }
    
    return info;
}

// Apply power profile
void applyPowerProfile(PowerProfile_t *profile) {
    xSemaphoreTake(xPowerMutex, portMAX_DELAY);
    
    currentProfile = *profile;
    printf("\n[PROFILE] Applying '%s' profile\n", profile->name);
    printf("  Sensor interval: %lu ms\n", profile->sensorInterval);
    printf("  Network: %s\n", profile->networkEnabled ? "Enabled" : "Disabled");
    printf("  Target saving: %.0f%%\n", profile->targetSaving);
    
    xSemaphoreGive(xPowerMutex);
}

// Select profile based on battery
void selectPowerProfile(void) {
    BatteryInfo_t battery = getBatteryInfo();
    
    if (battery.percentage < 20) {
        applyPowerProfile(&powerProfiles[3]);  // Ultra low power
        xEventGroupSetBits(xSystemEvents, LOW_BATTERY_BIT);
    } else if (battery.percentage < 40) {
        applyPowerProfile(&powerProfiles[2]);  // Power saver
    } else if (battery.percentage < 70) {
        applyPowerProfile(&powerProfiles[1]);  // Balanced
    } else {
        applyPowerProfile(&powerProfiles[0]);  // High performance
    }
}

// Sensor task - periodic wake
void vSensorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t readingCount = 0;
    
    printf("[SENSOR] Task started\n");
    
    for (;;) {
        // Simulate sensor reading
        readingCount++;
        printf("[SENSOR] Reading #%lu at tick %lu\n", 
               readingCount, xTaskGetTickCount());
        
        // Simulate data processing
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Signal data ready
        xEventGroupSetBits(xSystemEvents, SENSOR_DATA_READY_BIT);
        
        // Simulate wake source
        lastWakeSource = WAKE_SOURCE_TIMER;
        
        // Sleep until next reading
        vTaskDelayUntil(&xLastWakeTime, 
                       pdMS_TO_TICKS(currentProfile.sensorInterval));
        
        // System would enter tickless idle here if no other tasks ready
    }
}

// Network task - event driven
void vNetworkTask(void *pvParameters) {
    EventBits_t events;
    uint32_t packetCount = 0;
    
    printf("[NETWORK] Task started\n");
    
    for (;;) {
        // Wait for network events
        events = xEventGroupWaitBits(
            xSystemEvents,
            NETWORK_PACKET_BIT | SENSOR_DATA_READY_BIT,
            pdTRUE,  // Clear bits
            pdFALSE, // Wait for any
            currentProfile.networkEnabled ? portMAX_DELAY : pdMS_TO_TICKS(60000)
        );
        
        if (events & NETWORK_PACKET_BIT) {
            packetCount++;
            printf("[NETWORK] Received packet #%lu\n", packetCount);
            lastWakeSource = WAKE_SOURCE_NETWORK;
        }
        
        if (events & SENSOR_DATA_READY_BIT) {
            if (currentProfile.networkEnabled) {
                printf("[NETWORK] Transmitting sensor data\n");
                vTaskDelay(pdMS_TO_TICKS(100));  // Simulate transmission
            }
        }
        
        // Simulate occasional network activity
        if ((rand() % 100) < 5) {
            xEventGroupSetBits(xSystemEvents, NETWORK_PACKET_BIT);
        }
    }
}

// Logger task - batch processing
void vLoggerTask(void *pvParameters) {
    uint32_t logBuffer = 0;
    uint32_t flushCount = 0;
    
    printf("[LOGGER] Task started\n");
    
    for (;;) {
        // Accumulate logs
        EventBits_t events = xEventGroupWaitBits(
            xSystemEvents,
            SENSOR_DATA_READY_BIT | ALARM_TRIGGERED_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(30000)  // Flush every 30s
        );
        
        if (events & (SENSOR_DATA_READY_BIT | ALARM_TRIGGERED_BIT)) {
            logBuffer++;
        }
        
        // Batch write when buffer full or timeout
        if (logBuffer >= 10 || events == 0) {
            if (logBuffer > 0) {
                flushCount++;
                printf("[LOGGER] Flushing %lu entries (batch #%lu)\n", 
                       logBuffer, flushCount);
                vTaskDelay(pdMS_TO_TICKS(200));  // Simulate write
                logBuffer = 0;
            }
        }
    }
}

// Alarm task - critical wake events
void vAlarmTask(void *pvParameters) {
    uint32_t alarmCount = 0;
    
    printf("[ALARM] Task started\n");
    
    for (;;) {
        // Wait for alarm conditions
        EventBits_t events = xEventGroupWaitBits(
            xSystemEvents,
            LOW_BATTERY_BIT | ALARM_TRIGGERED_BIT,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        
        if (events & LOW_BATTERY_BIT) {
            alarmCount++;
            printf("[ALARM] LOW BATTERY WARNING! (%lu)\n", alarmCount);
            lastWakeSource = WAKE_SOURCE_ALARM;
        }
        
        if (events & ALARM_TRIGGERED_BIT) {
            alarmCount++;
            printf("[ALARM] CRITICAL ALARM! (%lu)\n", alarmCount);
            lastWakeSource = WAKE_SOURCE_ALARM;
        }
        
        // Simulate rare alarm
        if ((rand() % 1000) < 2) {
            xEventGroupSetBits(xSystemEvents, ALARM_TRIGGERED_BIT);
        }
    }
}

// Monitor task - power statistics
void vMonitorTask(void *pvParameters) {
    const TickType_t xPeriod = pdMS_TO_TICKS(10000);  // Every 10 seconds
    
    printf("[MONITOR] Task started\n");
    
    for (;;) {
        vTaskDelay(xPeriod);
        
        xSemaphoreTake(xPowerMutex, portMAX_DELAY);
        
        // Calculate statistics
        powerStats.totalTicks = xTaskGetTickCount();
        powerStats.powerSavingPercent = (powerStats.idleTicks * 100.0) / 
                                       powerStats.totalTicks;
        
        // Print power report
        printf("\n========================================\n");
        printf("POWER MANAGEMENT STATISTICS\n");
        printf("========================================\n");
        printf("Current Profile:     %s\n", currentProfile.name);
        printf("Total Runtime:       %lu ms\n", 
               powerStats.totalTicks * portTICK_PERIOD_MS);
        printf("Idle Time:          %lu ms\n", 
               powerStats.idleTicks * portTICK_PERIOD_MS);
        printf("Power Saving:       %.1f%% (target: %.0f%%)\n", 
               powerStats.powerSavingPercent, currentProfile.targetSaving);
        printf("Sleep Count:        %lu\n", powerStats.sleepCount);
        printf("Last Sleep:         %lu ms\n", 
               powerStats.lastSleepDuration * portTICK_PERIOD_MS);
        printf("Longest Sleep:      %lu ms\n", 
               powerStats.longestSleep * portTICK_PERIOD_MS);
        
        printf("\nWake Sources:\n");
        printf("  Timer:            %lu\n", powerStats.wakeCount[WAKE_SOURCE_TIMER]);
        printf("  Network:          %lu\n", powerStats.wakeCount[WAKE_SOURCE_NETWORK]);
        printf("  Sensor:           %lu\n", powerStats.wakeCount[WAKE_SOURCE_SENSOR]);
        printf("  Alarm:            %lu\n", powerStats.wakeCount[WAKE_SOURCE_ALARM]);
        printf("  Unknown:          %lu\n", powerStats.wakeCount[WAKE_SOURCE_UNKNOWN]);
        
        // Battery status
        BatteryInfo_t battery = getBatteryInfo();
        printf("\nBattery Status:\n");
        printf("  Voltage:          %u mV\n", battery.voltage_mV);
        printf("  Level:            %u%%\n", battery.percentage);
        printf("  Charging:         %s\n", battery.isCharging ? "Yes" : "No");
        
        // Estimate battery life
        float avgCurrent = 100.0 * (1.0 - powerStats.powerSavingPercent / 100.0) + 
                          2.0 * (powerStats.powerSavingPercent / 100.0);
        float batteryLife = (2000.0 / avgCurrent) * (battery.percentage / 100.0);
        printf("  Estimated Life:   %.1f hours\n", batteryLife);
        
        printf("========================================\n");
        
        xSemaphoreGive(xPowerMutex);
        
        // Adjust profile based on battery
        selectPowerProfile();
        
        // Simulate idle periods for testing
        if (currentProfile.aggressiveSleep) {
            // Force longer idle periods
            printf("[MONITOR] Entering extended idle period...\n");
            simulatePreSleepProcessing(pdMS_TO_TICKS(5000));
            vTaskDelay(pdMS_TO_TICKS(5000));
            simulatePostSleepProcessing(pdMS_TO_TICKS(5000));
        }
    }
}

// Test task to simulate activity
void vActivityTask(void *pvParameters) {
    printf("[ACTIVITY] Simulation task started\n");
    
    for (;;) {
        // Simulate periods of activity and idle
        for (int i = 0; i < 5; i++) {
            // Active period
            printf("[ACTIVITY] Busy period %d\n", i);
            for (int j = 0; j < 10; j++) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            // Idle period - let system sleep
            printf("[ACTIVITY] Idle period %d\n", i);
            simulatePreSleepProcessing(pdMS_TO_TICKS(2000));
            vTaskDelay(pdMS_TO_TICKS(2000));
            simulatePostSleepProcessing(pdMS_TO_TICKS(2000));
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

int main(void) {
    printf("===========================================\n");
    printf("Example 08: Power Management\n");
    printf("Tickless Idle Demonstration\n");
    printf("===========================================\n\n");
    
    printf("Features:\n");
    printf("  - Dynamic power profiles\n");
    printf("  - Battery-aware operation\n");
    printf("  - Multiple wake sources\n");
    printf("  - Power consumption tracking\n");
    printf("  - Idle time optimization\n\n");
    
    // Note about simulation
    printf("NOTE: This example simulates tickless idle\n");
    printf("      Real implementation requires hardware support\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create synchronization objects
    xSystemEvents = xEventGroupCreate();
    xPowerMutex = xSemaphoreCreateMutex();
    
    if (xSystemEvents == NULL || xPowerMutex == NULL) {
        printf("Failed to create sync objects!\n");
        return 1;
    }
    
    // Initialize with balanced profile
    applyPowerProfile(&powerProfiles[1]);
    
    // Create tasks
    TaskHandle_t xHandle;
    
    // Sensor task - periodic wake
    if (xTaskCreate(vSensorTask, "Sensor", SENSOR_TASK_STACK_SIZE,
                    NULL, 3, &xHandle) != pdPASS) {
        printf("Failed to create Sensor task!\n");
    }
    
    // Network task - event driven
    if (xTaskCreate(vNetworkTask, "Network", NETWORK_TASK_STACK_SIZE,
                    NULL, 2, &xHandle) != pdPASS) {
        printf("Failed to create Network task!\n");
    }
    
    // Logger task - batch processing
    if (xTaskCreate(vLoggerTask, "Logger", LOGGER_TASK_STACK_SIZE,
                    NULL, 1, &xHandle) != pdPASS) {
        printf("Failed to create Logger task!\n");
    }
    
    // Alarm task - critical events
    if (xTaskCreate(vAlarmTask, "Alarm", ALARM_TASK_STACK_SIZE,
                    NULL, 4, &xHandle) != pdPASS) {
        printf("Failed to create Alarm task!\n");
    }
    
    // Monitor task - statistics
    if (xTaskCreate(vMonitorTask, "Monitor", MONITOR_TASK_STACK_SIZE,
                    NULL, 1, &xHandle) != pdPASS) {
        printf("Failed to create Monitor task!\n");
    }
    
    // Activity simulation task
    if (xTaskCreate(vActivityTask, "Activity", 512,
                    NULL, 1, &xHandle) != pdPASS) {
        printf("Failed to create Activity task!\n");
    }
    
    printf("[MAIN] All tasks created\n");
    printf("[MAIN] Starting scheduler...\n\n");
    
    // Start the scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("Scheduler failed to start!\n");
    return 1;
}

// Required FreeRTOS hooks
void vApplicationMallocFailedHook(void) {
    printf("MALLOC FAILED!\n");
    for(;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    printf("STACK OVERFLOW: %s\n", pcTaskName);
    for(;;);
}

// Static allocation for idle and timer tasks
static StaticTask_t xIdleTaskTCB;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = xTimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}