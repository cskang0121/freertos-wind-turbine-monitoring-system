/*
 * Example 03: Queue-Based Producer-Consumer Pattern
 * 
 * This example demonstrates:
 * 1. Multiple producers at different rates
 * 2. Multiple consumers with different processing times
 * 3. Queue sizing and overflow handling
 * 4. Queue sets for monitoring multiple queues
 * 5. Performance statistics and monitoring
 * 
 * Wind turbine context:
 * - Fast producer: Vibration sensor (100Hz)
 * - Medium producer: Temperature sensor (10Hz)
 * - Burst producer: Event-based alerts
 * - Consumers: Data processing, logging, network transmission
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Queue sizes - based on producer rates and consumer latency */
#define SENSOR_QUEUE_SIZE       20  /* High-frequency sensor data */
#define PROCESSED_QUEUE_SIZE    10  /* Processed data for logging */
#define ALERT_QUEUE_SIZE        5   /* Critical alerts */

/* Task priorities */
#define FAST_PRODUCER_PRIORITY      (tskIDLE_PRIORITY + 3)
#define MEDIUM_PRODUCER_PRIORITY    (tskIDLE_PRIORITY + 2)
#define BURST_PRODUCER_PRIORITY     (tskIDLE_PRIORITY + 4)
#define PROCESSING_CONSUMER_PRIORITY (tskIDLE_PRIORITY + 5)
#define LOGGING_CONSUMER_PRIORITY    (tskIDLE_PRIORITY + 1)
#define NETWORK_CONSUMER_PRIORITY    (tskIDLE_PRIORITY + 6)
#define MONITOR_TASK_PRIORITY        (tskIDLE_PRIORITY + 1)

/* Sensor data structure */
typedef struct {
    uint32_t sequence;      /* Sequence number for tracking */
    uint32_t timestamp;     /* When data was produced */
    uint32_t producer_id;   /* Which producer sent this */
    float value;            /* Sensor value */
    uint8_t priority;       /* Data priority level */
} sensor_data_t;

/* Processed data structure */
typedef struct {
    uint32_t original_sequence;
    uint32_t processed_timestamp;
    float processed_value;
    float anomaly_score;
    uint8_t alert_level;    /* 0=normal, 1=warning, 2=critical */
} processed_data_t;

/* Global statistics */
typedef struct {
    uint32_t produced[3];       /* Count per producer */
    uint32_t consumed[3];       /* Count per consumer */
    uint32_t dropped;           /* Dropped due to full queue */
    uint32_t max_queue_usage;   /* Maximum queue depth seen */
    uint32_t total_latency;     /* Sum of all latencies */
    uint32_t latency_samples;   /* Number of latency measurements */
    uint32_t alerts_generated;  /* Number of alerts */
} stats_t;

static stats_t g_stats = {0};

/* Queue handles */
QueueHandle_t xSensorQueue = NULL;
QueueHandle_t xProcessedQueue = NULL;
QueueHandle_t xAlertQueue = NULL;
QueueSetHandle_t xQueueSet = NULL;

/* Mutex for statistics */
SemaphoreHandle_t xStatsMutex = NULL;

/*
 * Fast Producer Task - Simulates high-frequency vibration sensor (100Hz)
 */
void vFastProducerTask(void *pvParameters)
{
    (void)pvParameters;
    sensor_data_t data;
    uint32_t sequence = 0;
    const TickType_t xDelay = pdMS_TO_TICKS(10);  /* 100Hz */
    
    printf("[FAST PRODUCER] Started (100Hz vibration sensor)\n");
    
    /* Initialize random seed */
    srand(xTaskGetTickCount());
    
    for (;;) {
        /* Generate sensor data */
        data.sequence = sequence++;
        data.timestamp = xTaskGetTickCount();
        data.producer_id = 1;
        data.value = 50.0f + ((float)(rand() % 100) / 10.0f);  /* 50-60 range */
        data.priority = (data.value > 58.0f) ? 2 : 1;  /* High priority if > 58 */
        
        /* Try to send to queue (non-blocking with short timeout) */
        if (xQueueSend(xSensorQueue, &data, pdMS_TO_TICKS(5)) == pdTRUE) {
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.produced[0]++;
            xSemaphoreGive(xStatsMutex);
        } else {
            /* Queue full - data dropped */
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.dropped++;
            xSemaphoreGive(xStatsMutex);
            printf("[FAST PRODUCER] Queue full! Dropped sequence %lu\n", 
                   (unsigned long)data.sequence);
        }
        
        vTaskDelay(xDelay);
    }
}

/*
 * Medium Producer Task - Simulates temperature sensor (10Hz)
 */
void vMediumProducerTask(void *pvParameters)
{
    (void)pvParameters;
    sensor_data_t data;
    uint32_t sequence = 10000;  /* Different sequence range */
    const TickType_t xDelay = pdMS_TO_TICKS(100);  /* 10Hz */
    
    printf("[MEDIUM PRODUCER] Started (10Hz temperature sensor)\n");
    
    for (;;) {
        /* Generate sensor data */
        data.sequence = sequence++;
        data.timestamp = xTaskGetTickCount();
        data.producer_id = 2;
        data.value = 20.0f + ((float)(rand() % 50) / 10.0f);  /* 20-25 range */
        data.priority = (data.value > 24.0f) ? 2 : 0;  /* High priority if > 24 */
        
        /* Send to queue */
        if (xQueueSend(xSensorQueue, &data, pdMS_TO_TICKS(10)) == pdTRUE) {
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.produced[1]++;
            xSemaphoreGive(xStatsMutex);
        } else {
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.dropped++;
            xSemaphoreGive(xStatsMutex);
        }
        
        vTaskDelay(xDelay);
    }
}

/*
 * Burst Producer Task - Simulates event-based sensor (irregular)
 */
void vBurstProducerTask(void *pvParameters)
{
    (void)pvParameters;
    sensor_data_t data;
    uint32_t sequence = 20000;  /* Different sequence range */
    int burst_count;
    
    printf("[BURST PRODUCER] Started (event-based sensor)\n");
    
    for (;;) {
        /* Wait random time between bursts */
        vTaskDelay(pdMS_TO_TICKS(500 + (rand() % 2000)));
        
        /* Generate burst of data */
        burst_count = 3 + (rand() % 5);  /* 3-7 items */
        printf("[BURST PRODUCER] Generating burst of %d items\n", burst_count);
        
        for (int i = 0; i < burst_count; i++) {
            data.sequence = sequence++;
            data.timestamp = xTaskGetTickCount();
            data.producer_id = 3;
            data.value = 70.0f + ((float)(rand() % 300) / 10.0f);  /* 70-100 range */
            data.priority = 2;  /* Burst data is always high priority */
            
            if (xQueueSend(xSensorQueue, &data, 0) == pdTRUE) {
                xSemaphoreTake(xStatsMutex, portMAX_DELAY);
                g_stats.produced[2]++;
                xSemaphoreGive(xStatsMutex);
            } else {
                xSemaphoreTake(xStatsMutex, portMAX_DELAY);
                g_stats.dropped++;
                xSemaphoreGive(xStatsMutex);
                printf("[BURST PRODUCER] Queue full during burst!\n");
                break;  /* Stop burst if queue full */
            }
            
            vTaskDelay(pdMS_TO_TICKS(5));  /* Small delay between burst items */
        }
    }
}

/*
 * Processing Consumer Task - Analyzes sensor data and detects anomalies
 */
void vProcessingConsumerTask(void *pvParameters)
{
    (void)pvParameters;
    sensor_data_t sensor_data;
    processed_data_t processed_data;
    TickType_t latency;
    float baseline = 50.0f;
    
    printf("[PROCESSING CONSUMER] Started (anomaly detection)\n");
    
    for (;;) {
        /* Receive sensor data (blocking) */
        if (xQueueReceive(xSensorQueue, &sensor_data, portMAX_DELAY) == pdTRUE) {
            /* Calculate latency */
            latency = xTaskGetTickCount() - sensor_data.timestamp;
            
            /* Update statistics */
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.consumed[0]++;
            g_stats.total_latency += latency;
            g_stats.latency_samples++;
            xSemaphoreGive(xStatsMutex);
            
            /* Simulate processing (anomaly detection) */
            vTaskDelay(pdMS_TO_TICKS(2));  /* Processing time */
            
            /* Calculate anomaly score */
            float deviation = fabs(sensor_data.value - baseline);
            processed_data.anomaly_score = deviation / baseline * 100.0f;
            
            /* Determine alert level */
            if (processed_data.anomaly_score > 30.0f) {
                processed_data.alert_level = 2;  /* Critical */
            } else if (processed_data.anomaly_score > 20.0f) {
                processed_data.alert_level = 1;  /* Warning */
            } else {
                processed_data.alert_level = 0;  /* Normal */
            }
            
            /* Fill processed data */
            processed_data.original_sequence = sensor_data.sequence;
            processed_data.processed_timestamp = xTaskGetTickCount();
            processed_data.processed_value = sensor_data.value;
            
            /* Send to processed queue */
            xQueueSend(xProcessedQueue, &processed_data, 0);
            
            /* Send alert if needed */
            if (processed_data.alert_level > 0) {
                if (xQueueSend(xAlertQueue, &processed_data, 0) == pdTRUE) {
                    xSemaphoreTake(xStatsMutex, portMAX_DELAY);
                    g_stats.alerts_generated++;
                    xSemaphoreGive(xStatsMutex);
                    
                    printf("[PROCESSING] ALERT! Sequence %lu, Score %.1f%%, Level %d\n",
                           (unsigned long)sensor_data.sequence,
                           processed_data.anomaly_score,
                           processed_data.alert_level);
                }
            }
            
            /* Update baseline (simple moving average) */
            baseline = baseline * 0.95f + sensor_data.value * 0.05f;
        }
    }
}

/*
 * Logging Consumer Task - Logs processed data
 */
void vLoggingConsumerTask(void *pvParameters)
{
    (void)pvParameters;
    processed_data_t data;
    uint32_t log_count = 0;
    
    printf("[LOGGING CONSUMER] Started (data logger)\n");
    
    for (;;) {
        /* Receive processed data */
        if (xQueueReceive(xProcessedQueue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Simulate logging (slow operation) */
            vTaskDelay(pdMS_TO_TICKS(10));
            
            log_count++;
            
            /* Update statistics */
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.consumed[1]++;
            xSemaphoreGive(xStatsMutex);
            
            /* Print log every 10 items */
            if (log_count % 10 == 0) {
                printf("[LOGGING] Logged %lu items, latest: seq=%lu, value=%.1f\n",
                       (unsigned long)log_count,
                       (unsigned long)data.original_sequence,
                       data.processed_value);
            }
        }
    }
}

/*
 * Network Consumer Task - Sends alerts over network
 */
void vNetworkConsumerTask(void *pvParameters)
{
    (void)pvParameters;
    processed_data_t alert;
    
    printf("[NETWORK CONSUMER] Started (alert transmitter)\n");
    
    for (;;) {
        /* Receive alerts (high priority) */
        if (xQueueReceive(xAlertQueue, &alert, portMAX_DELAY) == pdTRUE) {
            /* Simulate network transmission */
            printf("[NETWORK] Transmitting alert: Level %d, Score %.1f%%\n",
                   alert.alert_level,
                   alert.anomaly_score);
            
            vTaskDelay(pdMS_TO_TICKS(50));  /* Network latency */
            
            /* Update statistics */
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            g_stats.consumed[2]++;
            xSemaphoreGive(xStatsMutex);
            
            printf("[NETWORK] Alert transmitted successfully\n");
        }
    }
}

/*
 * Queue Monitor Task - Uses queue set to monitor multiple queues
 */
void vQueueMonitorTask(void *pvParameters)
{
    (void)pvParameters;
    QueueSetMemberHandle_t xActivatedMember;
    sensor_data_t sensor_data;
    processed_data_t processed_data;
    UBaseType_t sensor_waiting, processed_waiting, alert_waiting;
    
    printf("[QUEUE MONITOR] Started (queue set monitoring)\n");
    
    /* Add queues to queue set */
    xQueueAddToSet(xSensorQueue, xQueueSet);
    xQueueAddToSet(xProcessedQueue, xQueueSet);
    
    for (;;) {
        /* Wait for any queue to have data (with timeout for periodic check) */
        xActivatedMember = xQueueSelectFromSet(xQueueSet, pdMS_TO_TICKS(1000));
        
        if (xActivatedMember == xSensorQueue) {
            /* Peek at sensor queue (don't remove) */
            if (xQueuePeek(xSensorQueue, &sensor_data, 0) == pdTRUE) {
                /* High priority data - could trigger immediate processing */
                if (sensor_data.priority == 2) {
                    printf("[MONITOR] High priority data in sensor queue!\n");
                }
            }
        } else if (xActivatedMember == xProcessedQueue) {
            /* Peek at processed queue */
            if (xQueuePeek(xProcessedQueue, &processed_data, 0) == pdTRUE) {
                if (processed_data.alert_level > 0) {
                    printf("[MONITOR] Alert in processed queue!\n");
                }
            }
        }
        
        /* Periodic queue statistics (every second) */
        static TickType_t last_report = 0;
        TickType_t now = xTaskGetTickCount();
        if (now - last_report > pdMS_TO_TICKS(1000)) {
            last_report = now;
            
            /* Get queue depths */
            sensor_waiting = uxQueueMessagesWaiting(xSensorQueue);
            processed_waiting = uxQueueMessagesWaiting(xProcessedQueue);
            alert_waiting = uxQueueMessagesWaiting(xAlertQueue);
            
            /* Update max usage */
            xSemaphoreTake(xStatsMutex, portMAX_DELAY);
            if (sensor_waiting > g_stats.max_queue_usage) {
                g_stats.max_queue_usage = sensor_waiting;
            }
            xSemaphoreGive(xStatsMutex);
            
            /* Print queue status */
            printf("[MONITOR] Queues: Sensor=%lu/%d, Processed=%lu/%d, Alert=%lu/%d\n",
                   (unsigned long)sensor_waiting, SENSOR_QUEUE_SIZE,
                   (unsigned long)processed_waiting, PROCESSED_QUEUE_SIZE,
                   (unsigned long)alert_waiting, ALERT_QUEUE_SIZE);
        }
    }
}

/*
 * Statistics Task - Reports overall system statistics
 */
void vStatisticsTask(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t xDelay = pdMS_TO_TICKS(5000);  /* Report every 5 seconds */
    uint32_t total_produced, total_consumed;
    float avg_latency;
    
    printf("[STATISTICS] Task started - Reports every 5 seconds\n");
    
    /* Initial delay */
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    for (;;) {
        xSemaphoreTake(xStatsMutex, portMAX_DELAY);
        
        /* Calculate totals */
        total_produced = g_stats.produced[0] + g_stats.produced[1] + g_stats.produced[2];
        total_consumed = g_stats.consumed[0] + g_stats.consumed[1] + g_stats.consumed[2];
        avg_latency = g_stats.latency_samples > 0 ? 
                      (float)g_stats.total_latency / g_stats.latency_samples : 0;
        
        printf("\n========================================\n");
        printf("QUEUE SYSTEM STATISTICS\n");
        printf("========================================\n");
        printf("Producers:\n");
        printf("  Fast (100Hz):   %lu items\n", (unsigned long)g_stats.produced[0]);
        printf("  Medium (10Hz):  %lu items\n", (unsigned long)g_stats.produced[1]);
        printf("  Burst:          %lu items\n", (unsigned long)g_stats.produced[2]);
        printf("  Total Produced: %lu items\n", (unsigned long)total_produced);
        printf("  Dropped:        %lu items (%.1f%%)\n", 
               (unsigned long)g_stats.dropped,
               total_produced > 0 ? (100.0f * g_stats.dropped / total_produced) : 0);
        printf("\nConsumers:\n");
        printf("  Processing:     %lu items\n", (unsigned long)g_stats.consumed[0]);
        printf("  Logging:        %lu items\n", (unsigned long)g_stats.consumed[1]);
        printf("  Network:        %lu items\n", (unsigned long)g_stats.consumed[2]);
        printf("  Total Consumed: %lu items\n", (unsigned long)total_consumed);
        printf("\nPerformance:\n");
        printf("  Avg Latency:    %.1f ticks\n", avg_latency);
        printf("  Max Queue Use:  %lu/%d items\n", 
               (unsigned long)g_stats.max_queue_usage, SENSOR_QUEUE_SIZE);
        printf("  Alerts:         %lu generated\n", (unsigned long)g_stats.alerts_generated);
        printf("  Efficiency:     %.1f%%\n",
               total_produced > 0 ? (100.0f * total_consumed / total_produced) : 0);
        printf("========================================\n\n");
        
        xSemaphoreGive(xStatsMutex);
        
        vTaskDelay(xDelay);
    }
}

int main(void)
{
    printf("\n===========================================\n");
    printf("Example 03: Queue-Based Producer-Consumer\n");
    printf("Multiple producers and consumers\n");
    printf("===========================================\n\n");
    
    /* Create queues */
    xSensorQueue = xQueueCreate(SENSOR_QUEUE_SIZE, sizeof(sensor_data_t));
    xProcessedQueue = xQueueCreate(PROCESSED_QUEUE_SIZE, sizeof(processed_data_t));
    xAlertQueue = xQueueCreate(ALERT_QUEUE_SIZE, sizeof(processed_data_t));
    
    if (xSensorQueue == NULL || xProcessedQueue == NULL || xAlertQueue == NULL) {
        printf("ERROR: Failed to create queues!\n");
        exit(1);
    }
    
    /* Create queue set for monitoring */
    xQueueSet = xQueueCreateSet(SENSOR_QUEUE_SIZE + PROCESSED_QUEUE_SIZE);
    if (xQueueSet == NULL) {
        printf("ERROR: Failed to create queue set!\n");
        exit(1);
    }
    
    /* Create statistics mutex */
    xStatsMutex = xSemaphoreCreateMutex();
    if (xStatsMutex == NULL) {
        printf("ERROR: Failed to create mutex!\n");
        exit(1);
    }
    
    /* Create producer tasks */
    if (xTaskCreate(vFastProducerTask, "FastProd", 
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    FAST_PRODUCER_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create fast producer!\n");
        exit(1);
    }
    
    if (xTaskCreate(vMediumProducerTask, "MedProd",
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    MEDIUM_PRODUCER_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create medium producer!\n");
        exit(1);
    }
    
    if (xTaskCreate(vBurstProducerTask, "BurstProd",
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    BURST_PRODUCER_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create burst producer!\n");
        exit(1);
    }
    
    /* Create consumer tasks */
    if (xTaskCreate(vProcessingConsumerTask, "Process",
                    configMINIMAL_STACK_SIZE * 3, NULL,
                    PROCESSING_CONSUMER_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create processing consumer!\n");
        exit(1);
    }
    
    if (xTaskCreate(vLoggingConsumerTask, "Logger",
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    LOGGING_CONSUMER_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create logging consumer!\n");
        exit(1);
    }
    
    if (xTaskCreate(vNetworkConsumerTask, "Network",
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    NETWORK_CONSUMER_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create network consumer!\n");
        exit(1);
    }
    
    /* Create monitoring tasks */
    if (xTaskCreate(vQueueMonitorTask, "QMonitor",
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    MONITOR_TASK_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create queue monitor!\n");
        exit(1);
    }
    
    if (xTaskCreate(vStatisticsTask, "Stats",
                    configMINIMAL_STACK_SIZE * 2, NULL,
                    MONITOR_TASK_PRIORITY, NULL) != pdPASS) {
        printf("ERROR: Failed to create statistics task!\n");
        exit(1);
    }
    
    printf("All tasks created successfully!\n");
    printf("Starting FreeRTOS scheduler...\n\n");
    
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
    /* Nothing to do */
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