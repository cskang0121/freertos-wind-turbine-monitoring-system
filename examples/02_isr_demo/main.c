/*
 * Example 02: Interrupt Service Routines with Deferred Processing
 * 
 * This example demonstrates:
 * 1. Simulated timer interrupts
 * 2. Minimal ISR design
 * 3. Deferred processing pattern using semaphores
 * 4. ISR to task communication via queues
 * 5. Latency measurement
 * 
 * In our wind turbine system, this pattern handles:
 * - Vibration sensor interrupts (anomaly detection)
 * - Emergency stop button (safety critical)
 * - Timer interrupts (periodic sampling)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

/* Simulation of hardware registers */
volatile uint32_t g_sensor_register = 0;
volatile uint32_t g_interrupt_count = 0;

/* Timing measurement */
typedef struct {
    TickType_t isr_timestamp;
    TickType_t task_timestamp;
    uint32_t latency_us;
} latency_stats_t;

/* Global statistics */
static struct {
    uint32_t total_interrupts;
    uint32_t processed_count;
    uint32_t max_latency_us;
    uint32_t min_latency_us;
    uint32_t avg_latency_us;
    uint32_t dropped_events;
} g_stats = {0, 0, 0, UINT32_MAX, 0, 0};

/* FreeRTOS handles */
SemaphoreHandle_t xBinarySemaphore = NULL;
QueueHandle_t xSensorDataQueue = NULL;
QueueHandle_t xEmergencyQueue = NULL;
TimerHandle_t xSimulatedTimer = NULL;

/* Task handles */
TaskHandle_t xDeferredTaskHandle = NULL;
TaskHandle_t xEmergencyTaskHandle = NULL;

/* Task priorities */
#define DEFERRED_TASK_PRIORITY    (tskIDLE_PRIORITY + 6)  /* High priority */
#define EMERGENCY_TASK_PRIORITY   (tskIDLE_PRIORITY + 7)  /* Highest priority */
#define MONITOR_TASK_PRIORITY     (tskIDLE_PRIORITY + 2)  /* Low priority */

/* Queue sizes */
#define SENSOR_QUEUE_SIZE         10
#define EMERGENCY_QUEUE_SIZE      5

/* Sensor data structure */
typedef struct {
    uint32_t value;
    TickType_t timestamp;
    uint32_t sequence;
} sensor_data_t;

/*
 * Simulated ISR - Timer Interrupt Handler
 * In real hardware, this would be triggered by a timer peripheral
 * 
 * Key principles demonstrated:
 * 1. Minimal processing (< 100 instructions)
 * 2. No blocking operations
 * 3. Use of FromISR APIs
 * 4. Context switch request when needed
 */
void vSimulatedISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static uint32_t sequence = 0;
    
    /* Increment interrupt counter */
    g_interrupt_count++;
    
    /* Simulate reading sensor register (minimal processing) */
    uint32_t sensor_value = g_sensor_register + (rand() % 100);
    
    /* Create sensor data packet */
    sensor_data_t data = {
        .value = sensor_value,
        .timestamp = xTaskGetTickCountFromISR(),
        .sequence = sequence++
    };
    
    /* Method 1: Binary Semaphore for simple signaling */
    xSemaphoreGiveFromISR(xBinarySemaphore, &xHigherPriorityTaskWoken);
    
    /* Method 2: Queue for data passing */
    if (xQueueSendFromISR(xSensorDataQueue, &data, &xHigherPriorityTaskWoken) != pdTRUE) {
        /* Queue full - data lost (track for statistics) */
        g_stats.dropped_events++;
    }
    
    /* Check for emergency condition (vibration > threshold) */
    if (sensor_value > 150) {
        /* Emergency detected - use high priority queue */
        xQueueSendFromISR(xEmergencyQueue, &data, &xHigherPriorityTaskWoken);
    }
    
    /* Update statistics */
    g_stats.total_interrupts++;
    
    /* Request context switch if a higher priority task was woken */
    /* Note: In simulation, this is handled by FreeRTOS port */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * Timer callback to simulate periodic interrupts
 * This simulates a 100Hz timer interrupt
 */
void vTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    
    /* Simulate hardware interrupt */
    vSimulatedISR();
}

/*
 * Deferred Processing Task
 * Handles the heavy processing that cannot be done in ISR context
 */
void vDeferredProcessingTask(void *pvParameters)
{
    (void)pvParameters;
    sensor_data_t received_data;
    TickType_t current_time;
    uint32_t latency_ticks;
    
    printf("[DEFERRED] Task started (Priority %d)\n", 
           (int)uxTaskPriorityGet(NULL));
    
    for (;;) {
        /* Wait for data from ISR */
        if (xQueueReceive(xSensorDataQueue, &received_data, portMAX_DELAY) == pdTRUE) {
            /* Get current time for latency measurement */
            current_time = xTaskGetTickCount();
            latency_ticks = current_time - received_data.timestamp;
            uint32_t latency_us = latency_ticks * (1000000 / configTICK_RATE_HZ);
            
            /* Update statistics */
            g_stats.processed_count++;
            if (latency_us > g_stats.max_latency_us) {
                g_stats.max_latency_us = latency_us;
            }
            if (latency_us < g_stats.min_latency_us) {
                g_stats.min_latency_us = latency_us;
            }
            g_stats.avg_latency_us = ((g_stats.avg_latency_us * (g_stats.processed_count - 1)) + 
                                      latency_us) / g_stats.processed_count;
            
            /* Simulate heavy processing */
            printf("[DEFERRED] Processing sensor data #%lu: value=%lu, latency=%luus\n",
                   (unsigned long)received_data.sequence,
                   (unsigned long)received_data.value,
                   (unsigned long)latency_us);
            
            /* Simulate complex calculations that would be inappropriate in ISR */
            volatile double result = 0;
            for (int i = 0; i < 1000; i++) {
                result += (double)received_data.value * 3.14159 / (i + 1);
            }
            
            /* Check for anomalies */
            if (received_data.value > 100) {
                printf("[DEFERRED] Warning: High vibration detected: %lu\n", 
                       (unsigned long)received_data.value);
            }
        }
    }
}

/*
 * Emergency Response Task
 * Handles critical interrupts with minimal latency
 */
void vEmergencyResponseTask(void *pvParameters)
{
    (void)pvParameters;
    sensor_data_t emergency_data;
    
    printf("[EMERGENCY] Task started (Priority %d) - CRITICAL\n", 
           (int)uxTaskPriorityGet(NULL));
    
    for (;;) {
        /* Wait for emergency events */
        if (xQueueReceive(xEmergencyQueue, &emergency_data, portMAX_DELAY) == pdTRUE) {
            /* Critical response - would trigger emergency stop in real system */
            printf("\n*** [EMERGENCY] CRITICAL EVENT ***\n");
            printf("*** Vibration level: %lu ***\n", (unsigned long)emergency_data.value);
            printf("*** Initiating emergency response ***\n");
            printf("*** Latency: %lu ticks ***\n\n", 
                   (unsigned long)(xTaskGetTickCount() - emergency_data.timestamp));
            
            /* In real system: Stop turbine, activate brakes, alert operator */
        }
    }
}

/*
 * Monitor Task - Reports statistics
 */
void vMonitorTask(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t xDelay = pdMS_TO_TICKS(5000);  /* Report every 5 seconds */
    
    printf("[MONITOR] Task started - Reports every 5 seconds\n");
    
    /* Initial delay */
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    for (;;) {
        printf("\n========================================\n");
        printf("ISR STATISTICS REPORT\n");
        printf("========================================\n");
        printf("Total Interrupts:    %lu\n", (unsigned long)g_stats.total_interrupts);
        printf("Processed Events:    %lu\n", (unsigned long)g_stats.processed_count);
        printf("Dropped Events:      %lu\n", (unsigned long)g_stats.dropped_events);
        printf("Max Latency:         %lu us\n", (unsigned long)g_stats.max_latency_us);
        printf("Min Latency:         %lu us\n", (unsigned long)g_stats.min_latency_us);
        printf("Avg Latency:         %lu us\n", (unsigned long)g_stats.avg_latency_us);
        printf("Processing Rate:     %.1f%%\n", 
               g_stats.total_interrupts > 0 ? 
               (100.0 * g_stats.processed_count / g_stats.total_interrupts) : 0);
        printf("========================================\n\n");
        
        vTaskDelay(xDelay);
    }
}

/*
 * Sensor simulation task - Updates sensor values
 */
void vSensorSimulationTask(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t xDelay = pdMS_TO_TICKS(100);  /* Update every 100ms */
    
    for (;;) {
        /* Simulate sensor value changes */
        g_sensor_register = 50 + (rand() % 100);
        
        /* Occasionally simulate high vibration */
        if ((rand() % 20) == 0) {
            g_sensor_register = 140 + (rand() % 50);  /* Emergency level */
        }
        
        vTaskDelay(xDelay);
    }
}

int main(void)
{
    printf("\n===========================================\n");
    printf("Example 02: ISR with Deferred Processing\n");
    printf("Simulating 100Hz timer interrupts\n");
    printf("===========================================\n\n");
    
    /* Create synchronization primitives */
    xBinarySemaphore = xSemaphoreCreateBinary();
    xSensorDataQueue = xQueueCreate(SENSOR_QUEUE_SIZE, sizeof(sensor_data_t));
    xEmergencyQueue = xQueueCreate(EMERGENCY_QUEUE_SIZE, sizeof(sensor_data_t));
    
    if (xBinarySemaphore == NULL || xSensorDataQueue == NULL || xEmergencyQueue == NULL) {
        printf("ERROR: Failed to create synchronization objects!\n");
        exit(1);
    }
    
    /* Create the deferred processing task (high priority) */
    if (xTaskCreate(vDeferredProcessingTask,
                    "Deferred",
                    configMINIMAL_STACK_SIZE * 3,
                    NULL,
                    DEFERRED_TASK_PRIORITY,
                    &xDeferredTaskHandle) != pdPASS) {
        printf("ERROR: Failed to create deferred task!\n");
        exit(1);
    }
    
    /* Create the emergency response task (highest priority) */
    if (xTaskCreate(vEmergencyResponseTask,
                    "Emergency",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    EMERGENCY_TASK_PRIORITY,
                    &xEmergencyTaskHandle) != pdPASS) {
        printf("ERROR: Failed to create emergency task!\n");
        exit(1);
    }
    
    /* Create monitor task */
    if (xTaskCreate(vMonitorTask,
                    "Monitor",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    MONITOR_TASK_PRIORITY,
                    NULL) != pdPASS) {
        printf("ERROR: Failed to create monitor task!\n");
        exit(1);
    }
    
    /* Create sensor simulation task */
    if (xTaskCreate(vSensorSimulationTask,
                    "SensorSim",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    tskIDLE_PRIORITY + 1,
                    NULL) != pdPASS) {
        printf("ERROR: Failed to create sensor simulation task!\n");
        exit(1);
    }
    
    /* Create timer to simulate periodic interrupts (100Hz = 10ms period) */
    xSimulatedTimer = xTimerCreate("ISR Timer",
                                   pdMS_TO_TICKS(10),  /* 10ms period */
                                   pdTRUE,             /* Auto-reload */
                                   NULL,               /* Timer ID */
                                   vTimerCallback);    /* Callback function */
    
    if (xSimulatedTimer == NULL) {
        printf("ERROR: Failed to create timer!\n");
        exit(1);
    }
    
    /* Start the timer */
    if (xTimerStart(xSimulatedTimer, 0) != pdPASS) {
        printf("ERROR: Failed to start timer!\n");
        exit(1);
    }
    
    printf("Starting FreeRTOS scheduler...\n");
    printf("Timer will trigger ISR at 100Hz\n\n");
    
    /* Start scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    printf("ERROR: Scheduler returned!\n");
    return 1;
}

/* FreeRTOS hook functions */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("ERROR: Stack overflow in task '%s'!\n", pcTaskName);
    exit(1);
}

void vApplicationMallocFailedHook(void)
{
    printf("ERROR: FreeRTOS malloc failed!\n");
    exit(1);
}

void vApplicationIdleHook(void)
{
    /* Nothing to do in idle */
}

/* Static allocation support */
#if configSUPPORT_STATIC_ALLOCATION == 1

static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if configUSE_TIMERS == 1
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif

#endif