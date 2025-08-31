/**
 * Example 05: Event Groups for Complex Synchronization
 * 
 * Demonstrates event groups for multi-condition synchronization
 * Shows AND/OR logic, broadcasting, and event-driven design
 * Simulates wind turbine system coordination
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"

// System Event Bits (0-7)
#define WIFI_CONNECTED_BIT      (1 << 0)  // 0x01
#define SENSORS_READY_BIT       (1 << 1)  // 0x02
#define CONFIG_LOADED_BIT       (1 << 2)  // 0x04
#define SYSTEM_INITIALIZED_BIT  (1 << 3)  // 0x08

// Operational Event Bits (8-15)
#define ANOMALY_DETECTED_BIT    (1 << 8)  // 0x100
#define DATA_READY_BIT          (1 << 9)  // 0x200
#define BUFFER_FULL_BIT         (1 << 10) // 0x400
#define TRANSMISSION_DONE_BIT   (1 << 11) // 0x800

// Safety Event Bits (16-23)
#define MAINTENANCE_MODE_BIT    (1 << 16) // 0x10000
#define EMERGENCY_STOP_BIT      (1 << 17) // 0x20000
#define OVERSPEED_ALARM_BIT     (1 << 18) // 0x40000
#define VIBRATION_ALARM_BIT     (1 << 19) // 0x80000

// Combined bit masks
#define SYSTEM_READY_BITS       (WIFI_CONNECTED_BIT | SENSORS_READY_BIT | CONFIG_LOADED_BIT)
#define ANY_ALARM_BITS          (OVERSPEED_ALARM_BIT | VIBRATION_ALARM_BIT)
#define TRANSMISSION_REQUIRED   (DATA_READY_BIT | WIFI_CONNECTED_BIT)

// Event groups
static EventGroupHandle_t xSystemEvents = NULL;
static EventGroupHandle_t xOperationalEvents = NULL;
static EventGroupHandle_t xSafetyEvents = NULL;

// Synchronization event group for barrier demonstration
static EventGroupHandle_t xSyncEvents = NULL;
#define TASK_A_SYNC_BIT (1 << 0)
#define TASK_B_SYNC_BIT (1 << 1)
#define TASK_C_SYNC_BIT (1 << 2)
#define ALL_SYNC_BITS   (TASK_A_SYNC_BIT | TASK_B_SYNC_BIT | TASK_C_SYNC_BIT)

// Statistics tracking
typedef struct {
    uint32_t events_set;
    uint32_t events_cleared;
    uint32_t anomalies_detected;
    uint32_t transmissions;
    uint32_t emergency_stops;
    uint32_t timeouts;
    TickType_t max_wait_time;
} Statistics_t;

static Statistics_t g_stats = {0};

// Helper function to print event bits
static void print_event_bits(const char *name, EventBits_t bits, uint8_t num_bits) {
    printf("[EVENTS] %s: 0x%06X = ", name, (unsigned int)bits);
    for (int i = num_bits - 1; i >= 0; i--) {
        printf("%d", (bits & (1 << i)) ? 1 : 0);
        if (i % 4 == 0 && i > 0) printf(" ");
    }
    printf("\n");
}

// Task 1: System Initialization
static void vInitTask(void *pvParameters) {
    (void)pvParameters;
    printf("[INIT] Starting system initialization...\n");
    
    // Simulate initialization sequence
    vTaskDelay(pdMS_TO_TICKS(500));
    printf("[INIT] Loading configuration...\n");
    xEventGroupSetBits(xSystemEvents, CONFIG_LOADED_BIT);
    g_stats.events_set++;
    
    vTaskDelay(pdMS_TO_TICKS(800));
    printf("[INIT] Initializing sensors...\n");
    xEventGroupSetBits(xSystemEvents, SENSORS_READY_BIT);
    g_stats.events_set++;
    
    vTaskDelay(pdMS_TO_TICKS(1200));
    printf("[INIT] Connecting to WiFi...\n");
    xEventGroupSetBits(xSystemEvents, WIFI_CONNECTED_BIT);
    g_stats.events_set++;
    
    // Wait for all initialization components
    EventBits_t bits = xEventGroupWaitBits(
        xSystemEvents,
        SYSTEM_READY_BITS,
        pdFALSE,  // Don't clear
        pdTRUE,   // Wait for ALL bits (AND)
        pdMS_TO_TICKS(5000)
    );
    
    if ((bits & SYSTEM_READY_BITS) == SYSTEM_READY_BITS) {
        printf("[INIT] System fully initialized!\n");
        xEventGroupSetBits(xSystemEvents, SYSTEM_INITIALIZED_BIT);
        g_stats.events_set++;
        print_event_bits("System Events", bits, 8);
    } else {
        printf("[INIT] Initialization timeout!\n");
        g_stats.timeouts++;
    }
    
    // Task complete
    vTaskDelete(NULL);
}

// Task 2: Sensor Monitoring (generates anomalies)
static void vSensorTask(void *pvParameters) {
    (void)pvParameters;
    uint32_t sample_count = 0;
    
    printf("[SENSOR] Task started\n");
    
    // Wait for system ready
    EventBits_t ready = xEventGroupWaitBits(
        xSystemEvents,
        SYSTEM_INITIALIZED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );
    
    printf("[SENSOR] System ready, starting monitoring\n");
    
    for (;;) {
        sample_count++;
        
        // Simulate sensor readings
        int vibration = 40 + (rand() % 30);
        int speed = 1400 + (rand() % 400);
        
        // Check for anomalies
        if (vibration > 60) {
            printf("[SENSOR] High vibration detected: %d\n", vibration);
            xEventGroupSetBits(xSafetyEvents, VIBRATION_ALARM_BIT);
            xEventGroupSetBits(xOperationalEvents, ANOMALY_DETECTED_BIT);
            g_stats.anomalies_detected++;
            g_stats.events_set += 2;
        }
        
        if (speed > 1700) {
            printf("[SENSOR] Overspeed detected: %d RPM\n", speed);
            xEventGroupSetBits(xSafetyEvents, OVERSPEED_ALARM_BIT);
            xEventGroupSetBits(xOperationalEvents, ANOMALY_DETECTED_BIT);
            g_stats.anomalies_detected++;
            g_stats.events_set += 2;
        }
        
        // Generate data ready event every 10 samples
        if (sample_count % 10 == 0) {
            xEventGroupSetBits(xOperationalEvents, DATA_READY_BIT);
            g_stats.events_set++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Task 3: Network Transmission (waits for multiple conditions)
static void vNetworkTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[NETWORK] Task started\n");
    
    for (;;) {
        // Wait for data ready AND wifi connected AND (anomaly OR buffer full)
        EventBits_t bits = xEventGroupWaitBits(
            xOperationalEvents,
            DATA_READY_BIT | ANOMALY_DETECTED_BIT | BUFFER_FULL_BIT,
            pdFALSE,  // Don't auto-clear yet
            pdFALSE,  // OR logic for triggers
            portMAX_DELAY
        );
        
        // Also check WiFi is connected
        EventBits_t system_bits = xEventGroupGetBits(xSystemEvents);
        if (!(system_bits & WIFI_CONNECTED_BIT)) {
            printf("[NETWORK] WiFi not connected, skipping transmission\n");
            continue;
        }
        
        // Determine transmission type
        if (bits & ANOMALY_DETECTED_BIT) {
            printf("[NETWORK] Transmitting PRIORITY anomaly data...\n");
            vTaskDelay(pdMS_TO_TICKS(200));  // Simulate transmission
            
            // Clear anomaly after transmission
            xEventGroupClearBits(xOperationalEvents, ANOMALY_DETECTED_BIT);
            g_stats.events_cleared++;
        } else if (bits & BUFFER_FULL_BIT) {
            printf("[NETWORK] Transmitting buffered data (buffer full)...\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            
            xEventGroupClearBits(xOperationalEvents, BUFFER_FULL_BIT);
            g_stats.events_cleared++;
        } else if (bits & DATA_READY_BIT) {
            printf("[NETWORK] Transmitting regular data...\n");
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        
        // Clear data ready bit and signal transmission done
        xEventGroupClearBits(xOperationalEvents, DATA_READY_BIT);
        xEventGroupSetBits(xOperationalEvents, TRANSMISSION_DONE_BIT);
        g_stats.transmissions++;
        g_stats.events_cleared++;
        g_stats.events_set++;
        
        // Auto-clear transmission done after brief period
        vTaskDelay(pdMS_TO_TICKS(100));
        xEventGroupClearBits(xOperationalEvents, TRANSMISSION_DONE_BIT);
        g_stats.events_cleared++;
    }
}

// Task 4: Safety Monitor (waits for ANY alarm)
static void vSafetyTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[SAFETY] Monitor started\n");
    
    for (;;) {
        // Wait for ANY alarm condition
        EventBits_t alarms = xEventGroupWaitBits(
            xSafetyEvents,
            ANY_ALARM_BITS | EMERGENCY_STOP_BIT,
            pdFALSE,  // Don't auto-clear (need to log)
            pdFALSE,  // OR logic - any alarm triggers
            pdMS_TO_TICKS(1000)
        );
        
        if (alarms & ANY_ALARM_BITS) {
            printf("[SAFETY] ALARM CONDITIONS DETECTED:\n");
            
            if (alarms & VIBRATION_ALARM_BIT) {
                printf("  - Vibration limit exceeded\n");
            }
            if (alarms & OVERSPEED_ALARM_BIT) {
                printf("  - Overspeed condition\n");
            }
            
            // Clear alarms after handling
            xEventGroupClearBits(xSafetyEvents, ANY_ALARM_BITS);
            g_stats.events_cleared++;
            
            // Check if we need emergency stop
            if ((alarms & VIBRATION_ALARM_BIT) && (alarms & OVERSPEED_ALARM_BIT)) {
                printf("[SAFETY] EMERGENCY STOP TRIGGERED!\n");
                xEventGroupSetBits(xSafetyEvents, EMERGENCY_STOP_BIT);
                g_stats.emergency_stops++;
                g_stats.events_set++;
            }
        }
        
        // Check for emergency stop
        if (alarms & EMERGENCY_STOP_BIT) {
            printf("[SAFETY] System in emergency stop state\n");
            // Would trigger actual emergency procedures here
            vTaskDelay(pdMS_TO_TICKS(5000));  // Simulate emergency handling
            
            // Clear emergency stop
            xEventGroupClearBits(xSafetyEvents, EMERGENCY_STOP_BIT);
            g_stats.events_cleared++;
            printf("[SAFETY] Emergency stop cleared, resuming operation\n");
        }
    }
}

// Task 5: Maintenance Mode Handler
static void vMaintenanceTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[MAINTENANCE] Handler started\n");
    
    for (;;) {
        // Periodically enter maintenance mode
        vTaskDelay(pdMS_TO_TICKS(15000));
        
        printf("[MAINTENANCE] Entering maintenance mode...\n");
        xEventGroupSetBits(xSafetyEvents, MAINTENANCE_MODE_BIT);
        g_stats.events_set++;
        
        // Maintenance activities
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        printf("[MAINTENANCE] Exiting maintenance mode\n");
        xEventGroupClearBits(xSafetyEvents, MAINTENANCE_MODE_BIT);
        g_stats.events_cleared++;
    }
}

// Synchronization barrier demonstration tasks
static void vSyncTaskA(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        printf("[SYNC-A] Reaching synchronization point...\n");
        
        EventBits_t bits = xEventGroupSync(
            xSyncEvents,
            TASK_A_SYNC_BIT,  // Set our bit
            ALL_SYNC_BITS,    // Wait for all tasks
            pdMS_TO_TICKS(5000)
        );
        
        if ((bits & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
            printf("[SYNC-A] All tasks synchronized!\n");
        } else {
            printf("[SYNC-A] Sync timeout\n");
            g_stats.timeouts++;
        }
    }
}

static void vSyncTaskB(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2500));
        printf("[SYNC-B] Reaching synchronization point...\n");
        
        EventBits_t bits = xEventGroupSync(
            xSyncEvents,
            TASK_B_SYNC_BIT,
            ALL_SYNC_BITS,
            pdMS_TO_TICKS(5000)
        );
        
        if ((bits & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
            printf("[SYNC-B] All tasks synchronized!\n");
        }
    }
}

static void vSyncTaskC(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        printf("[SYNC-C] Reaching synchronization point...\n");
        
        EventBits_t bits = xEventGroupSync(
            xSyncEvents,
            TASK_C_SYNC_BIT,
            ALL_SYNC_BITS,
            pdMS_TO_TICKS(5000)
        );
        
        if ((bits & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
            printf("[SYNC-C] All tasks synchronized!\n");
        }
    }
}

// Monitor task to display event states
static void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[MONITOR] Event monitor started\n");
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Get current state of all event groups
        EventBits_t system = xEventGroupGetBits(xSystemEvents);
        EventBits_t operational = xEventGroupGetBits(xOperationalEvents);
        EventBits_t safety = xEventGroupGetBits(xSafetyEvents);
        
        printf("\n");
        printf("========================================\n");
        printf("EVENT GROUP STATUS\n");
        printf("========================================\n");
        
        print_event_bits("System", system, 8);
        printf("  WiFi: %s, Sensors: %s, Config: %s, Init: %s\n",
               (system & WIFI_CONNECTED_BIT) ? "YES" : "NO",
               (system & SENSORS_READY_BIT) ? "YES" : "NO",
               (system & CONFIG_LOADED_BIT) ? "YES" : "NO",
               (system & SYSTEM_INITIALIZED_BIT) ? "YES" : "NO");
        
        print_event_bits("Operational", operational, 16);
        printf("  Anomaly: %s, Data: %s, Buffer: %s, Tx: %s\n",
               (operational & ANOMALY_DETECTED_BIT) ? "YES" : "NO",
               (operational & DATA_READY_BIT) ? "YES" : "NO",
               (operational & BUFFER_FULL_BIT) ? "YES" : "NO",
               (operational & TRANSMISSION_DONE_BIT) ? "YES" : "NO");
        
        print_event_bits("Safety", safety, 24);
        printf("  Maintenance: %s, Emergency: %s\n",
               (safety & MAINTENANCE_MODE_BIT) ? "YES" : "NO",
               (safety & EMERGENCY_STOP_BIT) ? "YES" : "NO");
        printf("  Overspeed: %s, Vibration: %s\n",
               (safety & OVERSPEED_ALARM_BIT) ? "YES" : "NO",
               (safety & VIBRATION_ALARM_BIT) ? "YES" : "NO");
        
        printf("\nStatistics:\n");
        printf("  Events Set:      %lu\n", g_stats.events_set);
        printf("  Events Cleared:  %lu\n", g_stats.events_cleared);
        printf("  Anomalies:       %lu\n", g_stats.anomalies_detected);
        printf("  Transmissions:   %lu\n", g_stats.transmissions);
        printf("  Emergency Stops: %lu\n", g_stats.emergency_stops);
        printf("  Timeouts:        %lu\n", g_stats.timeouts);
        printf("========================================\n");
        printf("\n");
    }
}

// FreeRTOS static allocation callbacks
#if configSUPPORT_STATIC_ALLOCATION == 1

static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif

// Application hooks
void vApplicationMallocFailedHook(void) {
    printf("[ERROR] Memory allocation failed!\n");
    for(;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    printf("[ERROR] Stack overflow in task: %s\n", pcTaskName);
    for(;;);
}

void vApplicationIdleHook(void) {
    // Can be used for power management
}

int main(void) {
    printf("===========================================\n");
    printf("Example 05: Event Groups\n");
    printf("Complex multi-condition synchronization\n");
    printf("===========================================\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create event groups
    xSystemEvents = xEventGroupCreate();
    xOperationalEvents = xEventGroupCreate();
    xSafetyEvents = xEventGroupCreate();
    xSyncEvents = xEventGroupCreate();
    
    if (xSystemEvents == NULL || xOperationalEvents == NULL || 
        xSafetyEvents == NULL || xSyncEvents == NULL) {
        printf("Failed to create event groups!\n");
        return 1;
    }
    
    printf("[MAIN] Event groups created:\n");
    printf("  - System Events (initialization, ready states)\n");
    printf("  - Operational Events (data, anomalies, transmission)\n");
    printf("  - Safety Events (alarms, emergency, maintenance)\n");
    printf("  - Sync Events (barrier synchronization demo)\n\n");
    
    // Create initialization task (runs once)
    xTaskCreate(vInitTask, "Init", configMINIMAL_STACK_SIZE * 2,
                NULL, 5, NULL);
    
    // Create operational tasks
    xTaskCreate(vSensorTask, "Sensor", configMINIMAL_STACK_SIZE * 2,
                NULL, 4, NULL);
    
    xTaskCreate(vNetworkTask, "Network", configMINIMAL_STACK_SIZE * 2,
                NULL, 3, NULL);
    
    xTaskCreate(vSafetyTask, "Safety", configMINIMAL_STACK_SIZE * 2,
                NULL, 6, NULL);  // Highest priority for safety
    
    xTaskCreate(vMaintenanceTask, "Maintenance", configMINIMAL_STACK_SIZE * 2,
                NULL, 2, NULL);
    
    // Create synchronization demonstration tasks
    xTaskCreate(vSyncTaskA, "SyncA", configMINIMAL_STACK_SIZE * 2,
                NULL, 3, NULL);
    
    xTaskCreate(vSyncTaskB, "SyncB", configMINIMAL_STACK_SIZE * 2,
                NULL, 3, NULL);
    
    xTaskCreate(vSyncTaskC, "SyncC", configMINIMAL_STACK_SIZE * 2,
                NULL, 3, NULL);
    
    // Create monitor task
    xTaskCreate(vMonitorTask, "Monitor", configMINIMAL_STACK_SIZE * 2,
                NULL, 1, NULL);
    
    printf("[MAIN] All tasks created, starting scheduler...\n\n");
    
    // Start the scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("Scheduler failed to start!\n");
    return 1;
}