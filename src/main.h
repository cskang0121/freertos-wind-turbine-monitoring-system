/*
 * Wind Turbine Predictive Maintenance System
 * Main Header File
 * 
 * This header contains common definitions and includes used throughout
 * the application. It serves as a central point for system-wide configurations.
 */

#ifndef MAIN_H
#define MAIN_H

/*-----------------------------------------------------------
 * Standard C Library Includes
 *----------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/*-----------------------------------------------------------
 * FreeRTOS Includes
 *----------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "timers.h"

/*-----------------------------------------------------------
 * Application Configuration
 *----------------------------------------------------------*/
#include "../config/app_config.h"

/*-----------------------------------------------------------
 * System Definitions
 *----------------------------------------------------------*/

/* System Version */
#define SYSTEM_VERSION_MAJOR    1
#define SYSTEM_VERSION_MINOR    0
#define SYSTEM_VERSION_PATCH    0
#define SYSTEM_VERSION_STRING   "1.0.0"

/* System States */
typedef enum {
    SYSTEM_STATE_INIT = 0,
    SYSTEM_STATE_READY,
    SYSTEM_STATE_RUNNING,
    SYSTEM_STATE_ERROR,
    SYSTEM_STATE_SLEEP
} system_state_t;

/* Error Codes */
typedef enum {
    ERROR_NONE = 0,
    ERROR_TASK_CREATE_FAILED,
    ERROR_QUEUE_CREATE_FAILED,
    ERROR_MUTEX_CREATE_FAILED,
    ERROR_MEMORY_ALLOCATION_FAILED,
    ERROR_STACK_OVERFLOW,
    ERROR_SENSOR_FAILURE,
    ERROR_NETWORK_FAILURE,
    ERROR_AI_INFERENCE_FAILED
} error_code_t;

/*-----------------------------------------------------------
 * Debug Macros
 *----------------------------------------------------------*/

/* Debug print macro - can be disabled in production */
#ifdef DEBUG_BUILD
    #define DEBUG_PRINT(fmt, ...) \
        printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

/* Error print macro - always enabled */
#define ERROR_PRINT(fmt, ...) \
    printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

/* Info print macro */
#define INFO_PRINT(fmt, ...) \
    printf("[INFO] " fmt "\n", ##__VA_ARGS__)

/*-----------------------------------------------------------
 * System Macros
 *----------------------------------------------------------*/

/* Min/Max macros */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Array size macro */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Bit manipulation macros */
#define BIT_SET(value, bit)     ((value) |= (1UL << (bit)))
#define BIT_CLEAR(value, bit)   ((value) &= ~(1UL << (bit)))
#define BIT_TOGGLE(value, bit)  ((value) ^= (1UL << (bit)))
#define BIT_CHECK(value, bit)   (((value) >> (bit)) & 1UL)

/*-----------------------------------------------------------
 * Global Variables (extern declarations)
 *----------------------------------------------------------*/

/* System state */
extern volatile system_state_t g_system_state;

/* Error tracking */
extern volatile error_code_t g_last_error;

/* Task handles - will be used throughout the application */
extern TaskHandle_t xSafetyTaskHandle;
extern TaskHandle_t xSensorTaskHandle;
extern TaskHandle_t xAnomalyTaskHandle;
extern TaskHandle_t xNetworkTaskHandle;

/* Queue handles */
extern QueueHandle_t xSensorDataQueue;
extern QueueHandle_t xAnomalyQueue;

/* Mutex handles */
extern SemaphoreHandle_t xSPIMutex;
extern SemaphoreHandle_t xUARTMutex;

/* Event group handle */
extern EventGroupHandle_t xSystemEventGroup;

/*-----------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------*/

/* System initialization */
void system_init(void);
void system_start(void);
void system_shutdown(void);

/* Error handling */
void error_handler(error_code_t error);
const char* error_to_string(error_code_t error);

/* Utility functions */
uint32_t get_system_time_ms(void);
void delay_ms(uint32_t milliseconds);

/*-----------------------------------------------------------
 * Platform-specific functions
 * These will be implemented differently for simulation vs hardware
 *----------------------------------------------------------*/

/* Hardware initialization */
void hardware_init(void);

/* LED control (for status indication) */
void led_on(uint8_t led_id);
void led_off(uint8_t led_id);
void led_toggle(uint8_t led_id);

/* Watchdog functions */
void watchdog_init(uint32_t timeout_ms);
void watchdog_feed(void);

#endif /* MAIN_H */
