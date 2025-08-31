/*
 * Example 01: Basic Tasks and Priority-Based Scheduling
 * 
 * This example demonstrates:
 * 1. Creating multiple FreeRTOS tasks
 * 2. Priority-based preemptive scheduling
 * 3. Task delays and timing
 * 4. How higher priority tasks preempt lower priority ones
 * 
 * Learning Objectives:
 * - Understand xTaskCreate() parameters
 * - See priority preemption in action
 * - Learn about task states (Running, Ready, Blocked)
 * - Practice with vTaskDelay()
 */

#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

/* Task priorities - Higher number = Higher priority */
#define LOW_PRIORITY_TASK     (tskIDLE_PRIORITY + 1)  /* Priority 1 */
#define MEDIUM_PRIORITY_TASK  (tskIDLE_PRIORITY + 2)  /* Priority 2 */  
#define HIGH_PRIORITY_TASK    (tskIDLE_PRIORITY + 3)  /* Priority 3 */

/* Task handles for reference */
TaskHandle_t xLowPriorityTaskHandle = NULL;
TaskHandle_t xMediumPriorityTaskHandle = NULL;
TaskHandle_t xHighPriorityTaskHandle = NULL;

/* Shared counter to demonstrate task execution order */
volatile uint32_t g_execution_counter = 0;

/*
 * Low Priority Task - Simulates background processing
 * In our wind turbine system, this might be logging or statistics
 */
void vLowPriorityTask(void *pvParameters)
{
    const char *pcTaskName = "LOW";
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(3000);  /* 3 second period */
    
    /* Initialize xLastWakeTime with current time */
    xLastWakeTime = xTaskGetTickCount();
    
    printf("[%s] Task started (Priority %d)\n", 
           pcTaskName, (int)uxTaskPriorityGet(NULL));
    
    for (;;) {
        g_execution_counter++;
        printf("[%s] Executing - Counter: %lu - Background work...\n", 
               pcTaskName, (unsigned long)g_execution_counter);
        
        /* Simulate some work */
        for (volatile int i = 0; i < 100000; i++) {
            /* Busy loop - in real system, this would be actual processing */
        }
        
        printf("[%s] Work completed, sleeping for 3 seconds\n", pcTaskName);
        
        /* Use vTaskDelayUntil for periodic execution */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/*
 * Medium Priority Task - Simulates sensor data processing
 * In our wind turbine system, this might be sensor data collection
 */
void vMediumPriorityTask(void *pvParameters)
{
    const char *pcTaskName = "MEDIUM";
    const TickType_t xDelay = pdMS_TO_TICKS(2000);  /* 2 second delay */
    
    printf("[%s] Task started (Priority %d)\n", 
           pcTaskName, (int)uxTaskPriorityGet(NULL));
    
    for (;;) {
        g_execution_counter++;
        printf("[%s] Executing - Counter: %lu - Processing sensors...\n", 
               pcTaskName, (unsigned long)g_execution_counter);
        
        /* Simulate sensor reading and processing */
        printf("[%s] Reading temperature: 25.5C\n", pcTaskName);
        printf("[%s] Reading vibration: 0.02g\n", pcTaskName);
        
        /* Notice: This task will preempt LOW priority task */
        printf("[%s] Processing complete, sleeping for 2 seconds\n", pcTaskName);
        
        vTaskDelay(xDelay);
    }
}

/*
 * High Priority Task - Simulates critical safety monitoring
 * In our wind turbine system, this would be safety-critical operations
 */
void vHighPriorityTask(void *pvParameters)
{
    const char *pcTaskName = "HIGH";
    const TickType_t xDelay = pdMS_TO_TICKS(1500);  /* 1.5 second delay */
    uint32_t ulEmergencyCheckCount = 0;
    
    printf("[%s] Task started (Priority %d) - SAFETY CRITICAL\n", 
           pcTaskName, (int)uxTaskPriorityGet(NULL));
    
    for (;;) {
        g_execution_counter++;
        ulEmergencyCheckCount++;
        
        printf("\n*** [%s] CRITICAL CHECK #%lu - Counter: %lu ***\n", 
               pcTaskName, 
               (unsigned long)ulEmergencyCheckCount,
               (unsigned long)g_execution_counter);
        
        /* Simulate critical safety checks */
        printf("*** [%s] Checking blade RPM... OK ***\n", pcTaskName);
        printf("*** [%s] Checking emergency stop... OK ***\n", pcTaskName);
        
        /* This task will preempt BOTH lower priority tasks */
        printf("*** [%s] Safety check complete ***\n\n", pcTaskName);
        
        vTaskDelay(xDelay);
    }
}

/*
 * Monitor Task - Reports system statistics
 * This demonstrates vTaskList() to show all task states
 */
void vMonitorTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(10000);  /* 10 second delay */
    char pcTaskListBuffer[512];
    
    printf("[MONITOR] Task started - Will report every 10 seconds\n");
    
    /* Initial delay to let other tasks start */
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    for (;;) {
        printf("\n========================================\n");
        printf("SYSTEM STATUS REPORT\n");
        printf("========================================\n");
        printf("Global Counter: %lu\n", (unsigned long)g_execution_counter);
        printf("\nTask List:\n");
        printf("Name          State   Prio    Stack   Num\n");
        printf("-------------------------------------------\n");
        
        /* Get task status information */
        vTaskList(pcTaskListBuffer);
        printf("%s", pcTaskListBuffer);
        
        printf("========================================\n\n");
        
        vTaskDelay(xDelay);
    }
}

int main(void)
{
    printf("\n============================================\n");
    printf("FreeRTOS Example 01: Basic Tasks & Priority\n");
    printf("============================================\n\n");
    
    printf("This example demonstrates:\n");
    printf("- Task creation with different priorities\n");
    printf("- Priority-based preemptive scheduling\n");
    printf("- How higher priority tasks interrupt lower ones\n\n");
    
    printf("Creating tasks...\n");
    
    /* Create Low Priority Task */
    if (xTaskCreate(vLowPriorityTask,
                    "Low",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    LOW_PRIORITY_TASK,
                    &xLowPriorityTaskHandle) != pdPASS) {
        printf("Failed to create Low Priority task!\n");
        exit(1);
    }
    
    /* Create Medium Priority Task */
    if (xTaskCreate(vMediumPriorityTask,
                    "Medium",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    MEDIUM_PRIORITY_TASK,
                    &xMediumPriorityTaskHandle) != pdPASS) {
        printf("Failed to create Medium Priority task!\n");
        exit(1);
    }
    
    /* Create High Priority Task */
    if (xTaskCreate(vHighPriorityTask,
                    "High",
                    configMINIMAL_STACK_SIZE * 2,
                    NULL,
                    HIGH_PRIORITY_TASK,
                    &xHighPriorityTaskHandle) != pdPASS) {
        printf("Failed to create High Priority task!\n");
        exit(1);
    }
    
    /* Create Monitor Task */
    if (xTaskCreate(vMonitorTask,
                    "Monitor",
                    configMINIMAL_STACK_SIZE * 4,  /* Larger stack for printf */
                    NULL,
                    tskIDLE_PRIORITY + 1,  /* Low priority */
                    NULL) != pdPASS) {
        printf("Failed to create Monitor task!\n");
        exit(1);
    }
    
    printf("All tasks created successfully!\n");
    printf("\nObserve how:\n");
    printf("1. HIGH priority task interrupts others\n");
    printf("2. MEDIUM priority task interrupts LOW\n");
    printf("3. Tasks with delays allow others to run\n");
    printf("\nStarting scheduler...\n\n");
    
    /* Start the scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    printf("ERROR: Scheduler returned!\n");
    for(;;);
    
    return 0;
}

/* FreeRTOS hook functions */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("STACK OVERFLOW in task: %s\n", pcTaskName);
    exit(1);
}

void vApplicationMallocFailedHook(void)
{
    printf("MALLOC FAILED!\n");
    exit(1);
}

void vApplicationIdleHook(void)
{
    /* Could enter low power mode here */
}

/* Static allocation support - required when configSUPPORT_STATIC_ALLOCATION is 1 */
#if configSUPPORT_STATIC_ALLOCATION == 1

/* Static memory for the idle task */
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
/* Static memory for the timer task */
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
#endif /* configUSE_TIMERS */

#endif /* configSUPPORT_STATIC_ALLOCATION */
