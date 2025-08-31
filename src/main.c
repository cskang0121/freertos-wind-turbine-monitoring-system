/*
 * Wind Turbine Predictive Maintenance System
 * Main Entry Point
 * 
 * This file demonstrates the foundation of our FreeRTOS-based system.
 * We start with simple task creation and scheduling to understand
 * the basics before building our complex predictive maintenance system.
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef SIMULATION_MODE
#include <time.h>  /* For nanosleep */
#endif

/* Task Handles - allows us to reference tasks after creation */
TaskHandle_t xHelloTaskHandle = NULL;
TaskHandle_t xCounterTaskHandle = NULL;

/* Task Functions - Forward declarations */
void vHelloTask(void *pvParameters);
void vCounterTask(void *pvParameters);

/*
 * Main function - Entry point of our application
 * In a real embedded system, this would be called after hardware initialization
 */
int main(void)
{
    printf("\n===========================================\n");
    printf("Wind Turbine Predictive Maintenance System\n");
    printf("FreeRTOS Learning Journey - Starting...\n");
    printf("===========================================\n\n");

    /* Create our first task - Simple Hello Task
     * This task will print messages and demonstrate basic task behavior
     * Parameters:
     * - Task function: vHelloTask
     * - Task name: "Hello" (for debugging)
     * - Stack size: configMINIMAL_STACK_SIZE * 2 (256 words = 1KB)
     * - Parameters: NULL (we're not passing any data to the task)
     * - Priority: 1 (just above idle task which is priority 0)
     * - Task handle: &xHelloTaskHandle (to reference the task later)
     */
    BaseType_t xReturned;
    xReturned = xTaskCreate(
        vHelloTask,                          /* Task function */
        "Hello",                             /* Task name (for debugging) */
        configMINIMAL_STACK_SIZE * 2,        /* Stack size in words */
        NULL,                                /* Task parameters */
        tskIDLE_PRIORITY + 1,                /* Task priority */
        &xHelloTaskHandle                    /* Task handle */
    );

    if (xReturned != pdPASS) {
        printf("ERROR: Failed to create Hello task!\n");
        exit(1);
    }

    /* Create a second task - Counter Task
     * This task has higher priority and will demonstrate preemption
     */
    xReturned = xTaskCreate(
        vCounterTask,                        /* Task function */
        "Counter",                           /* Task name */
        configMINIMAL_STACK_SIZE * 2,        /* Stack size */
        NULL,                                /* Task parameters */
        tskIDLE_PRIORITY + 2,                /* Higher priority than Hello task */
        &xCounterTaskHandle                  /* Task handle */
    );

    if (xReturned != pdPASS) {
        printf("ERROR: Failed to create Counter task!\n");
        exit(1);
    }

    printf("Tasks created successfully!\n");
    printf("Starting FreeRTOS scheduler...\n\n");

    /* Start the FreeRTOS scheduler
     * This function should never return if the scheduler starts successfully
     * From this point on, only tasks will run - main() is effectively done
     */
    vTaskStartScheduler();

    /* If we get here, the scheduler failed to start */
    printf("ERROR: Scheduler failed to start! Insufficient RAM?\n");
    
    /* Should never reach here */
    for(;;);
    
    return 0;
}

/*
 * Hello Task - Our first FreeRTOS task
 * This demonstrates:
 * - Basic task structure (infinite loop)
 * - Task delays using vTaskDelay()
 * - How tasks yield CPU time to other tasks
 */
void vHelloTask(void *pvParameters)
{
    /* Prevent compiler warning about unused parameter */
    (void) pvParameters;
    
    const TickType_t xDelay = pdMS_TO_TICKS(2000);  /* 2 second delay */
    uint32_t ulCount = 0;
    
    printf("[Hello] Task started! Priority: %d\n", 
           (int)uxTaskPriorityGet(xHelloTaskHandle));
    
    /* Task main loop - ALL tasks should have an infinite loop */
    for (;;) {
        ulCount++;
        printf("[Hello] Iteration %lu - Hello from FreeRTOS!\n", 
               (unsigned long)ulCount);
        
        /* Delay for 2 seconds
         * During this delay, other tasks can run
         * This is crucial for cooperative multitasking
         */
        vTaskDelay(xDelay);
    }
    
    /* Tasks should never return or exit */
}

/*
 * Counter Task - Demonstrates higher priority task behavior
 * This task has higher priority than Hello task, so it will
 * preempt (interrupt) the Hello task when it needs to run
 */
void vCounterTask(void *pvParameters)
{
    (void) pvParameters;
    
    const TickType_t xDelay = pdMS_TO_TICKS(1000);  /* 1 second delay */
    uint32_t ulCounter = 0;
    
    printf("[Counter] Task started! Priority: %d\n", 
           (int)uxTaskPriorityGet(xCounterTaskHandle));
    
    /* Task main loop */
    for (;;) {
        ulCounter++;
        
        /* Print counter value */
        printf("[Counter] Count: %lu", (unsigned long)ulCounter);
        
        /* Every 5 counts, print additional info */
        if (ulCounter % 5 == 0) {
            printf(" - Milestone reached!");
        }
        printf("\n");
        
        /* Delay for 1 second
         * Even though this task has higher priority, it still needs
         * to delay/block to allow lower priority tasks to run
         */
        vTaskDelay(xDelay);
    }
}

/*
 * FreeRTOS Hook Functions
 * These are callbacks that FreeRTOS will call in specific situations
 */

/* Called if a stack overflow is detected */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("ERROR: Stack overflow in task '%s'!\n", pcTaskName);
    /* In a real system, we would log this error and possibly reset */
    exit(1);
}

/* Called if malloc fails (when using heap_4.c) */
void vApplicationMallocFailedHook(void)
{
    printf("ERROR: FreeRTOS malloc failed! Out of heap memory.\n");
    /* In a real system, we would try to recover or reset */
    exit(1);
}

/* Idle task hook - called when no other tasks are running */
void vApplicationIdleHook(void)
{
    /* In a real embedded system, we could put the CPU to sleep here
     * For simulation, we'll just yield to the OS */
    #ifdef SIMULATION_MODE
    /* Give some time back to the host OS */
    struct timespec ts = {0, 1000000};  /* 1ms */
    nanosleep(&ts, NULL);
    #endif
}

/* Tick hook - called on every system tick (not used in this example) */
void vApplicationTickHook(void)
{
    /* We're not using the tick hook in this example */
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
