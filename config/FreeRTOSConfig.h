/*
 * FreeRTOS Configuration for Wind Turbine Predictive Maintenance System
 * Target: Mac/Linux POSIX Simulation (Educational Purpose)
 * 
 * This configuration demonstrates all 8 core capabilities:
 * 1. Task Scheduling with Preemption
 * 2. Interrupt Handling with Deferred Processing
 * 3. Queue-Based Communication
 * 4. Mutex Protection
 * 5. Event Groups
 * 6. Memory Management (Heap_4)
 * 7. Stack Overflow Detection
 * 8. Tickless Idle Power Management
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *----------------------------------------------------------*/

/* Capability 1: Task Scheduling Configuration */
#define configUSE_PREEMPTION                    1   // Enable preemptive scheduling
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0   // Use generic selection
#define configUSE_TIME_SLICING                  1   // Enable time slicing for equal priority tasks
#define configUSE_16_BIT_TICKS                  0   // Use 32-bit ticks
#define configMAX_PRIORITIES                    8   // 8 priority levels (0-7)
#define configIDLE_SHOULD_YIELD                 1   // Idle task yields to same priority tasks

/* CPU and Tick Configuration */
#ifdef SIMULATION_MODE
    #define configCPU_CLOCK_HZ                  ((unsigned long)1000000)  // 1MHz for simulation
#else
    /* Hardware configuration would go here if implemented */
    #define configCPU_CLOCK_HZ                  ((unsigned long)200000000) // Example: 200MHz
#endif
#define configTICK_RATE_HZ                      ((TickType_t)1000)      // 1ms tick rate
#define configMAX_TASK_NAME_LEN                 16                       // Task name length

/* Capability 6: Memory Management Configuration (Heap_4) */
#define configSUPPORT_STATIC_ALLOCATION         1   // Support static allocation
#define configSUPPORT_DYNAMIC_ALLOCATION        1   // Support dynamic allocation
#define configTOTAL_HEAP_SIZE                   ((size_t)(256 * 1024))  // 256KB heap
#define configAPPLICATION_ALLOCATED_HEAP        0   // FreeRTOS manages heap
#define configUSE_HEAP_SCHEME                   4   // Use heap_4 (with coalescing)

/* Stack Sizes (in words, not bytes) */
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)    // Minimal stack
#define configSAFETY_TASK_STACK_SIZE            ((unsigned short)512)    // 2KB for safety task
#define configANOMALY_TASK_STACK_SIZE           ((unsigned short)2048)   // 8KB for anomaly task
#define configNETWORK_TASK_STACK_SIZE           ((unsigned short)1024)   // 4KB for network
#define configSENSOR_TASK_STACK_SIZE            ((unsigned short)512)    // 2KB for sensors

/* Capability 7: Stack Overflow Detection */
#define configCHECK_FOR_STACK_OVERFLOW          2   // Method 2: Pattern checking
#define configUSE_MALLOC_FAILED_HOOK            1   // Enable malloc failed hook

/* Capability 2: Interrupt Configuration */
#ifdef SIMULATION_MODE
    #define configKERNEL_INTERRUPT_PRIORITY      255  // Lowest priority for simulation
    #define configMAX_SYSCALL_INTERRUPT_PRIORITY 191  // Higher priority for simulation
#else
    /* Example hardware configuration (not implemented) */
    #define configPRIO_BITS                      4    // 4 priority bits
    #define configLIBRARY_LOWEST_INTERRUPT_PRIORITY    15
    #define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY   5
    #define configKERNEL_INTERRUPT_PRIORITY      (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
    #define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#endif

/* Capability 3: Queue Configuration */
#define configQUEUE_REGISTRY_SIZE               10  // Track up to 10 queues for debugging
#define configUSE_QUEUE_SETS                    1   // Enable queue sets

/* Capability 4: Mutex Configuration */
#define configUSE_MUTEXES                       1   // Enable mutexes
#define configUSE_RECURSIVE_MUTEXES             1   // Enable recursive mutexes
#define configUSE_MUTEX_PRIORITY_INHERITANCE    1   // Enable priority inheritance

/* Capability 5: Event Groups Configuration */
#define configUSE_EVENT_GROUPS                  1   // Enable event groups

/* Semaphore Configuration */
#define configUSE_COUNTING_SEMAPHORES           1   // Enable counting semaphores
#define configUSE_BINARY_SEMAPHORES             1   // Binary semaphores (via semphr.h)

/* Capability 8: Power Management - Tickless Idle */
#define configUSE_TICKLESS_IDLE                 2   // Aggressive tickless idle
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   5   // Min 5 ticks before sleep
#define configUSE_IDLE_HOOK                     1   // Enable idle hook for power management
#define configUSE_TICK_HOOK                     0   // Not using tick hook

/* Co-routine definitions (not used in this project) */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/* Software Timer Configuration */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* Optional Features */
#define configUSE_TASK_NOTIFICATIONS            1   // Enable task notifications
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3   // 3 notification values per task
#define configUSE_TRACE_FACILITY                1   // Enable trace macros
#define configUSE_STATS_FORMATTING_FUNCTIONS    1   // Enable vTaskList() and vTaskGetRunTimeStats()
#define configGENERATE_RUN_TIME_STATS           1   // Enable runtime statistics

/* Runtime Stats Timer Configuration */
#ifdef SIMULATION_MODE
    /* For POSIX port simulation */
    extern void vConfigureTimerForRunTimeStats(void);
    extern unsigned long ulGetRunTimeCounterValue(void);
    #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() vConfigureTimerForRunTimeStats()
    #define portGET_RUN_TIME_COUNTER_VALUE() ulGetRunTimeCounterValue()
#else
    /* Use AmebaPro2 hardware timer */
    #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() 
    /* Note: Hardware timer implementation would be required for real hardware */
    #define portGET_RUN_TIME_COUNTER_VALUE() 0  // Placeholder for hardware implementation
#endif

/* Hook Function Prototypes */
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Assert and Error Checking */
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

/* Include Functionality */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_uxTaskGetStackHighWaterMark2    1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTaskResumeFromISR              1
#define INCLUDE_xQueueGetMutexHolder            1

/* Memory Protection Unit (MPU) Configuration (not used in simulation) */
#define configTOTAL_MPU_REGIONS                 8   // Example: 8 MPU regions
#define configTEX_S_C_B_FLASH                   0x07UL
#define configTEX_S_C_B_SRAM                    0x07UL

/* Definitions that map FreeRTOS Port interrupts */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

/* Tickless Idle Configuration for Power Management */
#ifdef configUSE_TICKLESS_IDLE
    #define configPRE_SLEEP_PROCESSING(x)       vPreSleepProcessing(x)
    #define configPOST_SLEEP_PROCESSING(x)      vPostSleepProcessing(x)
    
    /* Function prototypes for power management */
    void vPreSleepProcessing(uint32_t ulExpectedIdleTime);
    void vPostSleepProcessing(uint32_t ulExpectedIdleTime);
#endif

/* Thread Local Storage */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* Stream Buffer Configuration */
#define configSTREAM_BUFFER_TRIGGER_LEVEL_TEST_MARGIN 2

#endif /* FREERTOS_CONFIG_H */
