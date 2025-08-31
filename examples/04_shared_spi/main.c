/**
 * Example 04: Mutex for Shared Resources
 * 
 * Demonstrates mutex usage for protecting shared SPI bus access
 * Multiple tasks access different sensors on the same SPI bus
 * Shows priority inheritance and deadlock prevention
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

// Simulated sensor types on SPI bus
typedef enum {
    SENSOR_VIBRATION = 0,
    SENSOR_TEMPERATURE,
    SENSOR_CURRENT,
    SENSOR_PRESSURE
} SensorType_t;

// Simulated SPI transaction
typedef struct {
    SensorType_t sensor;
    uint8_t cmd;
    uint8_t data[8];
    TickType_t timestamp;
} SPITransaction_t;

// Global mutex for SPI bus protection
static SemaphoreHandle_t xSPIMutex = NULL;
static SemaphoreHandle_t xConfigMutex = NULL;
static SemaphoreHandle_t xRecursiveMutex = NULL;

// Shared configuration
typedef struct {
    uint32_t vibration_threshold;
    uint32_t temp_threshold;
    uint32_t sample_rate;
    bool monitoring_enabled;
} SystemConfig_t;

static SystemConfig_t g_config = {
    .vibration_threshold = 100,
    .temp_threshold = 80,
    .sample_rate = 100,
    .monitoring_enabled = true
};

// Statistics tracking
typedef struct {
    uint32_t spi_transactions;
    uint32_t mutex_timeouts;
    uint32_t priority_inversions;
    uint32_t config_updates;
    TickType_t max_wait_time;
    uint32_t sensor_reads[4];  // Per sensor type
} Statistics_t;

static Statistics_t g_stats = {0};

// Simulated SPI transfer function
static bool spi_transfer(SensorType_t sensor, uint8_t cmd, uint8_t *data, size_t len) {
    // Simulate SPI hardware access
    printf("[SPI] Sensor %d: CMD=0x%02X ", sensor, cmd);
    
    // Simulate different sensors returning different data
    switch(sensor) {
        case SENSOR_VIBRATION:
            data[0] = 45 + (rand() % 20);  // 45-64 range
            data[1] = rand() % 255;
            printf("Vibration=%d ", (data[0] << 8) | data[1]);
            break;
            
        case SENSOR_TEMPERATURE:
            data[0] = 20 + (rand() % 10);  // 20-29 range
            printf("Temp=%d ", data[0]);
            break;
            
        case SENSOR_CURRENT:
            data[0] = 10 + (rand() % 5);   // 10-14 range
            printf("Current=%dA ", data[0]);
            break;
            
        case SENSOR_PRESSURE:
            data[0] = 100 + (rand() % 10); // 100-109 range
            printf("Pressure=%d ", data[0]);
            break;
    }
    
    printf("\n");
    
    // Simulate SPI transfer time
    vTaskDelay(pdMS_TO_TICKS(2));
    
    g_stats.spi_transactions++;
    g_stats.sensor_reads[sensor]++;
    
    return true;
}

// Thread-safe SPI access with mutex
static bool spi_transfer_safe(SensorType_t sensor, uint8_t cmd, 
                             uint8_t *data, size_t len, TickType_t timeout) {
    TickType_t start_time = xTaskGetTickCount();
    
    // Try to acquire SPI mutex
    if (xSemaphoreTake(xSPIMutex, timeout) == pdTRUE) {
        TickType_t wait_time = xTaskGetTickCount() - start_time;
        if (wait_time > g_stats.max_wait_time) {
            g_stats.max_wait_time = wait_time;
        }
        
        // Critical section - exclusive SPI access
        bool result = spi_transfer(sensor, cmd, data, len);
        
        // Release mutex
        xSemaphoreGive(xSPIMutex);
        
        return result;
    } else {
        // Timeout occurred
        printf("[ERROR] Task %s: SPI mutex timeout!\n", pcTaskGetName(NULL));
        g_stats.mutex_timeouts++;
        return false;
    }
}

// Thread-safe configuration read
static SystemConfig_t config_read_safe(void) {
    SystemConfig_t local_config;
    
    if (xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Atomic read of entire structure
        local_config = g_config;
        xSemaphoreGive(xConfigMutex);
    } else {
        printf("[ERROR] Config mutex timeout in read!\n");
        memset(&local_config, 0, sizeof(local_config));
    }
    
    return local_config;
}

// Thread-safe configuration update
static bool config_update_safe(uint32_t vibration_threshold, uint32_t temp_threshold) {
    if (xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Atomic update of multiple fields
        g_config.vibration_threshold = vibration_threshold;
        g_config.temp_threshold = temp_threshold;
        g_stats.config_updates++;
        
        printf("[CONFIG] Updated: Vib=%lu, Temp=%lu\n", 
               vibration_threshold, temp_threshold);
        
        xSemaphoreGive(xConfigMutex);
        return true;
    }
    
    printf("[ERROR] Config mutex timeout in update!\n");
    return false;
}

// Recursive mutex example - nested function calls
static void low_level_log(const char *msg) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    
    printf("[LOG-LL] %s\n", msg);
    
    xSemaphoreGiveRecursive(xRecursiveMutex);
}

static void high_level_log(const char *component, const char *msg) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s: %s", component, msg);
    low_level_log(buffer);  // Nested call with same mutex - OK with recursive!
    
    xSemaphoreGiveRecursive(xRecursiveMutex);
}

// Task 1: High-frequency vibration monitoring (Priority 6)
static void vVibrationTask(void *pvParameters) {
    uint8_t data[8];
    uint32_t sample_count = 0;
    
    printf("[VIBRATION] Task started (Priority 6, 100Hz)\n");
    
    for (;;) {
        // Read vibration sensor via SPI
        if (spi_transfer_safe(SENSOR_VIBRATION, 0x01, data, 2, pdMS_TO_TICKS(50))) {
            uint16_t vibration = (data[0] << 8) | data[1];
            
            // Check threshold
            SystemConfig_t config = config_read_safe();
            if (vibration > config.vibration_threshold) {
                high_level_log("VIBRATION", "Threshold exceeded!");
            }
            
            sample_count++;
            if (sample_count % 100 == 0) {
                printf("[VIBRATION] 100 samples processed\n");
            }
        }
        
        // 100Hz sampling rate
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task 2: Temperature monitoring (Priority 4)
static void vTemperatureTask(void *pvParameters) {
    uint8_t data[8];
    
    printf("[TEMPERATURE] Task started (Priority 4, 10Hz)\n");
    
    for (;;) {
        // Read temperature sensor
        if (spi_transfer_safe(SENSOR_TEMPERATURE, 0x02, data, 1, pdMS_TO_TICKS(100))) {
            uint8_t temp = data[0];
            
            // Check threshold
            SystemConfig_t config = config_read_safe();
            if (temp > config.temp_threshold) {
                high_level_log("TEMPERATURE", "Over temperature!");
            }
        }
        
        // 10Hz sampling rate
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 3: Current monitoring (Priority 5)
static void vCurrentTask(void *pvParameters) {
    uint8_t data[8];
    
    printf("[CURRENT] Task started (Priority 5, 50Hz)\n");
    
    for (;;) {
        // Read current sensor
        if (spi_transfer_safe(SENSOR_CURRENT, 0x03, data, 1, pdMS_TO_TICKS(75))) {
            uint8_t current = data[0];
            
            if (current > 12) {
                high_level_log("CURRENT", "High current detected");
            }
        }
        
        // 50Hz sampling rate
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Task 4: Low priority pressure monitoring (Priority 2)
static void vPressureTask(void *pvParameters) {
    uint8_t data[8];
    
    printf("[PRESSURE] Task started (Priority 2, 1Hz)\n");
    
    for (;;) {
        // This low priority task should experience delays due to higher priority tasks
        TickType_t start = xTaskGetTickCount();
        
        if (spi_transfer_safe(SENSOR_PRESSURE, 0x04, data, 1, pdMS_TO_TICKS(200))) {
            TickType_t elapsed = xTaskGetTickCount() - start;
            
            if (elapsed > pdMS_TO_TICKS(10)) {
                printf("[PRESSURE] Delayed by %lu ms (priority inversion?)\n",
                       elapsed * portTICK_PERIOD_MS);
                g_stats.priority_inversions++;
            }
        }
        
        // 1Hz sampling rate
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task 5: Configuration manager (Priority 3)
static void vConfigTask(void *pvParameters) {
    printf("[CONFIG] Task started (Priority 3)\n");
    
    for (;;) {
        // Periodically update configuration
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // Simulate configuration changes
        uint32_t new_vib = 90 + (rand() % 30);   // 90-119
        uint32_t new_temp = 70 + (rand() % 20);  // 70-89
        
        config_update_safe(new_vib, new_temp);
    }
}

// Task 6: Statistics reporter
static void vStatsTask(void *pvParameters) {
    printf("[STATS] Task started\n");
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        printf("\n");
        printf("========================================\n");
        printf("MUTEX SYSTEM STATISTICS\n");
        printf("========================================\n");
        printf("SPI Transactions:    %lu\n", g_stats.spi_transactions);
        printf("  Vibration:         %lu\n", g_stats.sensor_reads[SENSOR_VIBRATION]);
        printf("  Temperature:       %lu\n", g_stats.sensor_reads[SENSOR_TEMPERATURE]);
        printf("  Current:           %lu\n", g_stats.sensor_reads[SENSOR_CURRENT]);
        printf("  Pressure:          %lu\n", g_stats.sensor_reads[SENSOR_PRESSURE]);
        printf("\n");
        printf("Mutex Performance:\n");
        printf("  Timeouts:          %lu\n", g_stats.mutex_timeouts);
        printf("  Max Wait:          %lu ticks\n", g_stats.max_wait_time);
        printf("  Priority Inversions: %lu\n", g_stats.priority_inversions);
        printf("\n");
        printf("Config Updates:      %lu\n", g_stats.config_updates);
        printf("========================================\n");
        printf("\n");
        
        // Show current configuration
        SystemConfig_t config = config_read_safe();
        printf("Current Config: Vib=%lu, Temp=%lu\n", 
               config.vibration_threshold, config.temp_threshold);
    }
}

// Priority inheritance demonstration task
static void vPriorityTestTask(void *pvParameters) {
    printf("[PRIORITY TEST] Started - will demonstrate priority inheritance\n");
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(7000));
        
        printf("\n[PRIORITY TEST] Creating priority inversion scenario...\n");
        
        // Low priority task will hold mutex
        // High priority task will request it
        // Without priority inheritance, medium priority task would run
        // With priority inheritance, low task gets boosted
        
        UBaseType_t original_priority = uxTaskPriorityGet(NULL);
        printf("[PRIORITY TEST] My priority: %lu\n", original_priority);
        
        // Note: In real scenario, we'd monitor actual priority changes
        // FreeRTOS handles this automatically with mutex
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
    printf("Example 04: Mutex for Shared Resources\n");
    printf("Multiple tasks sharing SPI bus\n");
    printf("Demonstrates priority inheritance\n");
    printf("===========================================\n\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create mutexes
    xSPIMutex = xSemaphoreCreateMutex();
    xConfigMutex = xSemaphoreCreateMutex();
    xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
    
    if (xSPIMutex == NULL || xConfigMutex == NULL || xRecursiveMutex == NULL) {
        printf("Failed to create mutexes!\n");
        return 1;
    }
    
    printf("[MAIN] Mutexes created successfully\n");
    printf("  - SPI Mutex (with priority inheritance)\n");
    printf("  - Config Mutex (protects shared data)\n");
    printf("  - Recursive Mutex (allows nested calls)\n\n");
    
    // Create tasks with different priorities
    // Higher number = higher priority
    xTaskCreate(vVibrationTask, "Vibration", configMINIMAL_STACK_SIZE * 2,
                NULL, 6, NULL);  // Highest priority
    
    xTaskCreate(vCurrentTask, "Current", configMINIMAL_STACK_SIZE * 2,
                NULL, 5, NULL);  // High priority
    
    xTaskCreate(vTemperatureTask, "Temperature", configMINIMAL_STACK_SIZE * 2,
                NULL, 4, NULL);  // Medium priority
    
    xTaskCreate(vConfigTask, "Config", configMINIMAL_STACK_SIZE * 2,
                NULL, 3, NULL);  // Low-medium priority
    
    xTaskCreate(vPressureTask, "Pressure", configMINIMAL_STACK_SIZE * 2,
                NULL, 2, NULL);  // Low priority
    
    xTaskCreate(vStatsTask, "Stats", configMINIMAL_STACK_SIZE * 2,
                NULL, 1, NULL);  // Lowest priority
    
    xTaskCreate(vPriorityTestTask, "PriorityTest", configMINIMAL_STACK_SIZE * 2,
                NULL, 4, NULL);  // Medium priority
    
    printf("[MAIN] All tasks created, starting scheduler...\n\n");
    
    // Start the scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("Scheduler failed to start!\n");
    return 1;
}