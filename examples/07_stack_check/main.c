/**
 * Example 07: Stack Overflow Detection
 * 
 * Demonstrates stack overflow detection methods,
 * stack monitoring, and safe stack sizing strategies
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Stack sizes for different tasks
#define SMALL_STACK_SIZE    (configMINIMAL_STACK_SIZE)      // Minimal
#define NORMAL_STACK_SIZE   (configMINIMAL_STACK_SIZE * 2)  // Standard
#define LARGE_STACK_SIZE    (configMINIMAL_STACK_SIZE * 4)  // Generous
#define HUGE_STACK_SIZE     (configMINIMAL_STACK_SIZE * 8)  // Printf-heavy

// Stack monitoring structure
typedef struct {
    TaskHandle_t handle;
    const char* name;
    size_t configured_size;
    UBaseType_t initial_high_water;
    UBaseType_t current_high_water;
    UBaseType_t minimum_high_water;
    uint8_t peak_usage_percent;
    bool warning_issued;
    uint32_t check_count;
} StackMonitor_t;

#define MAX_MONITORED_TASKS 10
static StackMonitor_t g_stack_monitors[MAX_MONITORED_TASKS] = {0};
static int g_monitor_count = 0;
static SemaphoreHandle_t xMonitorMutex = NULL;

// Global flags for controlled testing
static bool g_enable_recursion_test = false;
static bool g_enable_array_test = false;
static bool g_enable_printf_test = false;
static int g_recursion_depth = 0;
static int g_max_recursion_reached = 0;

// Forward declarations
static void register_task_monitor(TaskHandle_t handle, const char* name, size_t stack_size);
static void update_stack_monitors(void);
static void print_stack_report(void);
static uint8_t calculate_stack_usage_percent(UBaseType_t high_water, size_t total_size);

// Register a task for stack monitoring
static void register_task_monitor(TaskHandle_t handle, const char* name, size_t stack_size) {
    if (g_monitor_count < MAX_MONITORED_TASKS) {
        g_stack_monitors[g_monitor_count].handle = handle;
        g_stack_monitors[g_monitor_count].name = name;
        g_stack_monitors[g_monitor_count].configured_size = stack_size;
        g_stack_monitors[g_monitor_count].initial_high_water = uxTaskGetStackHighWaterMark(handle);
        g_stack_monitors[g_monitor_count].current_high_water = g_stack_monitors[g_monitor_count].initial_high_water;
        g_stack_monitors[g_monitor_count].minimum_high_water = g_stack_monitors[g_monitor_count].initial_high_water;
        g_stack_monitors[g_monitor_count].peak_usage_percent = 0;
        g_stack_monitors[g_monitor_count].warning_issued = false;
        g_stack_monitors[g_monitor_count].check_count = 0;
        g_monitor_count++;
    }
}

// Update all stack monitors
static void update_stack_monitors(void) {
    for (int i = 0; i < g_monitor_count; i++) {
        if (g_stack_monitors[i].handle != NULL) {
            g_stack_monitors[i].current_high_water = uxTaskGetStackHighWaterMark(g_stack_monitors[i].handle);
            g_stack_monitors[i].check_count++;
            
            // Track minimum (worst case)
            if (g_stack_monitors[i].current_high_water < g_stack_monitors[i].minimum_high_water) {
                g_stack_monitors[i].minimum_high_water = g_stack_monitors[i].current_high_water;
                
                // Calculate usage percentage
                g_stack_monitors[i].peak_usage_percent = calculate_stack_usage_percent(
                    g_stack_monitors[i].minimum_high_water,
                    g_stack_monitors[i].configured_size
                );
                
                // Issue warning if usage > 80%
                if (g_stack_monitors[i].peak_usage_percent > 80 && !g_stack_monitors[i].warning_issued) {
                    printf("\n[WARNING] Task '%s' stack usage > 80%% (%u%%)\n",
                           g_stack_monitors[i].name,
                           g_stack_monitors[i].peak_usage_percent);
                    g_stack_monitors[i].warning_issued = true;
                }
            }
        }
    }
}

// Calculate stack usage percentage
static uint8_t calculate_stack_usage_percent(UBaseType_t high_water, size_t total_size) {
    size_t words_total = total_size / sizeof(StackType_t);
    size_t words_used = words_total - high_water;
    return (uint8_t)((words_used * 100) / words_total);
}

// Print visual stack usage bar
static void print_stack_bar(uint8_t percent) {
    printf("[");
    for (int i = 0; i < 20; i++) {
        if (i < (percent / 5)) {
            printf("#");
        } else {
            printf("-");
        }
    }
    printf("] %3u%%", percent);
}

// Print comprehensive stack report
static void print_stack_report(void) {
    printf("\n");
    printf("========================================\n");
    printf("STACK USAGE REPORT\n");
    printf("========================================\n");
    printf("%-15s %8s %8s %8s %8s %6s\n",
           "Task", "Size", "Used", "Free", "Min Free", "Usage");
    printf("%-15s %8s %8s %8s %8s %6s\n",
           "----", "----", "----", "----", "--------", "-----");
    
    for (int i = 0; i < g_monitor_count; i++) {
        if (g_stack_monitors[i].handle != NULL) {
            size_t words_total = g_stack_monitors[i].configured_size / sizeof(StackType_t);
            size_t words_used = words_total - g_stack_monitors[i].current_high_water;
            size_t words_min_free = g_stack_monitors[i].minimum_high_water;
            
            printf("%-15s %8zu %8zu %8zu %8zu ",
                   g_stack_monitors[i].name,
                   g_stack_monitors[i].configured_size,
                   words_used * sizeof(StackType_t),
                   g_stack_monitors[i].current_high_water * sizeof(StackType_t),
                   words_min_free * sizeof(StackType_t));
            
            print_stack_bar(g_stack_monitors[i].peak_usage_percent);
            
            if (g_stack_monitors[i].peak_usage_percent > 90) {
                printf(" CRITICAL!");
            } else if (g_stack_monitors[i].peak_usage_percent > 80) {
                printf(" WARNING!");
            } else if (g_stack_monitors[i].peak_usage_percent > 70) {
                printf(" Caution");
            }
            
            printf("\n");
        }
    }
    
    printf("========================================\n");
    printf("Note: Sizes in bytes. Lower 'Min Free' = higher usage\n");
    printf("========================================\n\n");
}

// Task 1: Minimal stack usage (baseline)
static void vMinimalTask(void *pvParameters) {
    (void)pvParameters;
    int counter = 0;
    
    printf("[MINIMAL] Task started with %d bytes stack\n", SMALL_STACK_SIZE);
    register_task_monitor(xTaskGetCurrentTaskHandle(), "Minimal", SMALL_STACK_SIZE);
    
    for (;;) {
        // Minimal stack usage - just increment counter
        counter++;
        
        if (counter % 1000 == 0) {
            // Even printf uses stack!
            printf("[MINIMAL] Count: %d\n", counter);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 2: Moderate stack usage
static void vModerateTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[MODERATE] Task started with %d bytes stack\n", NORMAL_STACK_SIZE);
    register_task_monitor(xTaskGetCurrentTaskHandle(), "Moderate", NORMAL_STACK_SIZE);
    
    for (;;) {
        // Moderate local variables
        char buffer[128];
        int data[32];
        
        // Initialize data
        for (int i = 0; i < 32; i++) {
            data[i] = rand() % 100;
        }
        
        // Format string (uses stack)
        snprintf(buffer, sizeof(buffer), 
                 "Data: %d, %d, %d, %d", 
                 data[0], data[1], data[2], data[3]);
        
        if (strlen(buffer) > 0) {  // Prevent optimization
            // Occasional output
            if (rand() % 100 == 0) {
                printf("[MODERATE] %s\n", buffer);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Task 3: Heavy stack usage (printf, formatting)
static void vHeavyTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[HEAVY] Task started with %d bytes stack\n", LARGE_STACK_SIZE);
    register_task_monitor(xTaskGetCurrentTaskHandle(), "Heavy", LARGE_STACK_SIZE);
    
    for (;;) {
        // Large local arrays
        float sensor_data[64];
        char output[256];
        
        // Generate sensor data
        for (int i = 0; i < 64; i++) {
            sensor_data[i] = (float)(rand() % 1000) / 10.0f;
        }
        
        // Heavy formatting (printf with floats uses lots of stack!)
        if (g_enable_printf_test) {
            printf("[HEAVY] Sensor readings:\n");
            for (int i = 0; i < 8; i++) {
                printf("  [%d]: %.2f, %.2f, %.2f, %.2f\n",
                       i,
                       sensor_data[i*4],
                       sensor_data[i*4+1],
                       sensor_data[i*4+2],
                       sensor_data[i*4+3]);
            }
        }
        
        // Calculate statistics
        float sum = 0, min = sensor_data[0], max = sensor_data[0];
        for (int i = 0; i < 64; i++) {
            sum += sensor_data[i];
            if (sensor_data[i] < min) min = sensor_data[i];
            if (sensor_data[i] > max) max = sensor_data[i];
        }
        
        snprintf(output, sizeof(output),
                 "Stats: Avg=%.2f, Min=%.2f, Max=%.2f",
                 sum/64, min, max);
        
        if (rand() % 50 == 0) {
            printf("[HEAVY] %s\n", output);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Recursive function for testing
static int recursive_function(int depth, bool print) {
    // Local variables consume stack
    char local_buffer[64];
    int local_data[8];
    
    g_recursion_depth = depth;
    if (depth > g_max_recursion_reached) {
        g_max_recursion_reached = depth;
    }
    
    // Fill locals to ensure stack usage
    snprintf(local_buffer, sizeof(local_buffer), "Depth: %d", depth);
    for (int i = 0; i < 8; i++) {
        local_data[i] = depth * i;
    }
    
    if (print && (depth % 10 == 0)) {
        printf("[RECURSION] %s, Stack: %zu words free\n",
               local_buffer,
               uxTaskGetStackHighWaterMark(NULL));
    }
    
    // Recurse deeper
    if (depth > 0) {
        return recursive_function(depth - 1, print) + local_data[0];
    }
    
    return local_data[7];
}

// Task 4: Recursion test task
static void vRecursionTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[RECURSION] Task started with %d bytes stack\n", LARGE_STACK_SIZE);
    register_task_monitor(xTaskGetCurrentTaskHandle(), "Recursion", LARGE_STACK_SIZE);
    
    for (;;) {
        if (g_enable_recursion_test) {
            printf("[RECURSION] Starting recursion test...\n");
            
            // Start with shallow recursion
            int depth = 10;
            int result = recursive_function(depth, true);
            
            printf("[RECURSION] Completed depth %d, result: %d\n", depth, result);
            printf("[RECURSION] Max depth reached: %d\n", g_max_recursion_reached);
            
            g_enable_recursion_test = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task 5: Large array test (controlled overflow)
static void vArrayTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[ARRAY] Task started with %d bytes stack\n", NORMAL_STACK_SIZE);
    register_task_monitor(xTaskGetCurrentTaskHandle(), "Array", NORMAL_STACK_SIZE);
    
    for (;;) {
        if (g_enable_array_test) {
            printf("[ARRAY] Testing large array allocation...\n");
            
            // Check current stack
            UBaseType_t before = uxTaskGetStackHighWaterMark(NULL);
            printf("[ARRAY] Stack before: %zu words free\n", before);
            
            // WARNING: This might overflow on small stacks!
            // Allocate array based on available stack
            size_t safe_size = (before * sizeof(StackType_t)) / 2;  // Use half of free
            
            if (safe_size > 16) {
                // Use VLA (Variable Length Array) - dangerous!
                volatile uint8_t test_array[safe_size];
                
                // Fill array to ensure it's allocated
                for (size_t i = 0; i < safe_size; i++) {
                    test_array[i] = (uint8_t)(i & 0xFF);
                }
                
                UBaseType_t after = uxTaskGetStackHighWaterMark(NULL);
                printf("[ARRAY] Stack after: %zu words free\n", after);
                printf("[ARRAY] Used %zu bytes for array\n", 
                       (before - after) * sizeof(StackType_t));
            } else {
                printf("[ARRAY] Not enough stack for safe test!\n");
            }
            
            g_enable_array_test = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task 6: Monitor task
static void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    
    printf("[MONITOR] Stack monitor task started\n");
    register_task_monitor(xTaskGetCurrentTaskHandle(), "Monitor", NORMAL_STACK_SIZE);
    
    // Wait for tasks to start
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    for (;;) {
        // Update all monitors
        if (xSemaphoreTake(xMonitorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            update_stack_monitors();
            xSemaphoreGive(xMonitorMutex);
        }
        
        // Print report every 5 seconds
        static int report_counter = 0;
        if (++report_counter >= 50) {  // 50 * 100ms = 5 seconds
            print_stack_report();
            report_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 7: Control task (enables tests)
static void vControlTask(void *pvParameters) {
    (void)pvParameters;
    int test_cycle = 0;
    
    printf("[CONTROL] Test control task started\n");
    printf("[CONTROL] Tests will run periodically...\n\n");
    
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Every 10 seconds
        
        test_cycle++;
        printf("\n[CONTROL] Starting test cycle %d\n", test_cycle);
        
        // Rotate through tests
        switch (test_cycle % 3) {
            case 0:
                printf("[CONTROL] Enabling recursion test (safe depth)\n");
                g_enable_recursion_test = true;
                break;
                
            case 1:
                printf("[CONTROL] Enabling array allocation test\n");
                g_enable_array_test = true;
                break;
                
            case 2:
                printf("[CONTROL] Enabling heavy printf test\n");
                g_enable_printf_test = true;
                vTaskDelay(pdMS_TO_TICKS(2000));
                g_enable_printf_test = false;
                break;
        }
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

// Stack overflow hook - CRITICAL!
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // CRITICAL: Stack has overflowed!
    // System is likely corrupted
    
    // Try to output error (might fail if stack badly corrupted)
    printf("\n\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!! STACK OVERFLOW DETECTED !!\n");
    printf("!! Task: %s\n", pcTaskName);
    printf("!! System halted for safety !!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    
    // Disable interrupts and halt
    taskDISABLE_INTERRUPTS();
    
    // In production, might trigger watchdog reset instead
    for(;;) {
        // Halt system - stack overflow is critical failure
    }
}

void vApplicationIdleHook(void) {
    // Could check stack usage here
}

// Function to demonstrate stack painting
static void demonstrate_stack_painting(void) {
    printf("\n[DEMO] Stack Painting Visualization:\n");
    printf("  Unused stack filled with: 0xA5\n");  // FreeRTOS uses 0xA5A5A5A5 pattern
    printf("  High water mark = deepest stack usage\n");
    printf("  Pattern intact = stack never used\n");
    printf("  Pattern gone = stack was used\n\n");
}

int main(void) {
    printf("===========================================\n");
    printf("Example 07: Stack Overflow Detection\n");
    printf("Monitoring and protection demonstration\n");
    printf("===========================================\n\n");
    fflush(stdout);
    
    // Show configuration
    printf("Configuration:\n");
    printf("  Stack overflow check: ");
    #if (configCHECK_FOR_STACK_OVERFLOW == 0)
        printf("DISABLED\n");
    #elif (configCHECK_FOR_STACK_OVERFLOW == 1)
        printf("Method 1 (Quick check)\n");
    #elif (configCHECK_FOR_STACK_OVERFLOW == 2)
        printf("Method 2 (Pattern check)\n");
    #endif
    
    printf("  Stack sizes:\n");
    printf("    MINIMAL:  %d bytes\n", SMALL_STACK_SIZE);
    printf("    NORMAL:   %d bytes\n", NORMAL_STACK_SIZE);
    printf("    LARGE:    %d bytes\n", LARGE_STACK_SIZE);
    printf("    HUGE:     %d bytes\n", HUGE_STACK_SIZE);
    printf("\n");
    
    // Demonstrate stack painting
    demonstrate_stack_painting();
    fflush(stdout);
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create monitor mutex
    xMonitorMutex = xSemaphoreCreateMutex();
    if (xMonitorMutex == NULL) {
        printf("Failed to create monitor mutex!\n");
        return 1;
    }
    
    // Create tasks with different stack requirements
    TaskHandle_t xHandle;
    
    // Minimal stack task
    if (xTaskCreate(vMinimalTask, "Minimal", SMALL_STACK_SIZE,
                    NULL, 2, &xHandle) != pdPASS) {
        printf("Failed to create Minimal task!\n");
    }
    
    // Moderate stack task
    if (xTaskCreate(vModerateTask, "Moderate", NORMAL_STACK_SIZE,
                    NULL, 2, &xHandle) != pdPASS) {
        printf("Failed to create Moderate task!\n");
    }
    
    // Heavy stack task
    if (xTaskCreate(vHeavyTask, "Heavy", LARGE_STACK_SIZE,
                    NULL, 3, &xHandle) != pdPASS) {
        printf("Failed to create Heavy task!\n");
    }
    
    // Recursion test task
    if (xTaskCreate(vRecursionTask, "Recursion", LARGE_STACK_SIZE,
                    NULL, 2, &xHandle) != pdPASS) {
        printf("Failed to create Recursion task!\n");
    }
    
    // Array test task
    if (xTaskCreate(vArrayTask, "Array", NORMAL_STACK_SIZE,
                    NULL, 2, &xHandle) != pdPASS) {
        printf("Failed to create Array task!\n");
    }
    
    // Monitor task
    if (xTaskCreate(vMonitorTask, "Monitor", NORMAL_STACK_SIZE,
                    NULL, 4, &xHandle) != pdPASS) {
        printf("Failed to create Monitor task!\n");
    }
    
    // Control task
    if (xTaskCreate(vControlTask, "Control", NORMAL_STACK_SIZE,
                    NULL, 1, &xHandle) != pdPASS) {
        printf("Failed to create Control task!\n");
    }
    
    printf("[MAIN] All tasks created successfully\n");
    printf("[MAIN] Starting scheduler...\n\n");
    fflush(stdout);
    
    // Start the scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("Scheduler failed to start!\n");
    return 1;
}