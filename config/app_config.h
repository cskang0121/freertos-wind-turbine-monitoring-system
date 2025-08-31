/*
 * Application Configuration for Wind Turbine Predictive Maintenance
 * 
 * This file contains all application-specific settings including:
 * - Task priorities and stack sizes
 * - Queue sizes and configurations
 * - Sensor parameters
 * - AI model settings
 * - Network configuration
 * - Threshold values
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*-----------------------------------------------------------
 * Task Configuration (Capability 1: Task Scheduling)
 *----------------------------------------------------------*/

/* Task Priorities (0 = lowest, 7 = highest) */
#define SAFETY_TASK_PRIORITY        (tskIDLE_PRIORITY + 7)  // Highest - Emergency response
#define SENSOR_TASK_PRIORITY        (tskIDLE_PRIORITY + 6)  // High - Real-time data
#define ANOMALY_TASK_PRIORITY       (tskIDLE_PRIORITY + 5)  // Medium-High - AI processing
#define WIFI_MANAGER_TASK_PRIORITY  (tskIDLE_PRIORITY + 3)  // Medium - Connection management
#define NETWORK_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)  // Low - Data transmission
#define LOGGING_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)  // Low - SD card logging
#define STACK_MONITOR_PRIORITY      (tskIDLE_PRIORITY + 1)  // Very Low - Monitoring

/* Task Stack Sizes (in words, not bytes) */
#define SAFETY_TASK_STACK_SIZE      (512)   // 2KB - Simple logic
#define SENSOR_TASK_STACK_SIZE      (512)   // 2KB - Data acquisition
#define ANOMALY_TASK_STACK_SIZE     (2048)  // 8KB - AI inference + FFT
#define NETWORK_TASK_STACK_SIZE     (1024)  // 4KB - Network buffers
#define WIFI_MANAGER_STACK_SIZE     (768)   // 3KB - WiFi management
#define LOGGING_TASK_STACK_SIZE     (512)   // 2KB - File operations
#define STACK_MONITOR_STACK_SIZE    (256)   // 1KB - Minimal

/*-----------------------------------------------------------
 * Queue Configuration (Capability 3: Queues)
 *----------------------------------------------------------*/

/* Queue Sizes */
#define SENSOR_QUEUE_LENGTH         10      // 10 sensor packets
#define ANOMALY_QUEUE_LENGTH        5       // 5 anomaly reports
#define LOG_QUEUE_LENGTH           20      // 20 log messages
#define COMMAND_QUEUE_LENGTH        5       // 5 commands

/* Queue Item Sizes */
#define SENSOR_DATA_SIZE           sizeof(sensor_data_t)
#define ANOMALY_REPORT_SIZE        sizeof(anomaly_report_t)
#define LOG_MESSAGE_SIZE           128     // Max log message length

/*-----------------------------------------------------------
 * Synchronization Configuration (Capabilities 4 & 5)
 *----------------------------------------------------------*/

/* Event Group Bits (Capability 5) */
#define ANOMALY_DETECTED_BIT       (1 << 0)
#define WIFI_CONNECTED_BIT         (1 << 1)
#define CLOUD_AUTH_BIT            (1 << 2)
#define SLEEP_REQUEST_BIT         (1 << 3)
#define SENSOR_SLEEP_ACK_BIT      (1 << 4)
#define NETWORK_SLEEP_ACK_BIT     (1 << 5)
#define ALL_READY_BITS            (ANOMALY_DETECTED_BIT | WIFI_CONNECTED_BIT | CLOUD_AUTH_BIT)

/* Mutex Timeout Values (Capability 4) */
#define SPI_MUTEX_TIMEOUT_MS       100     // 100ms timeout for SPI access
#define SD_MUTEX_TIMEOUT_MS        500     // 500ms timeout for SD card
#define UART_MUTEX_TIMEOUT_MS      50      // 50ms timeout for UART

/*-----------------------------------------------------------
 * Sensor Configuration
 *----------------------------------------------------------*/

/* Sampling Rates */
#define VIBRATION_SAMPLE_RATE_HZ   1000    // 1kHz vibration sampling
#define SENSOR_TASK_RATE_HZ        100     // 100Hz sensor task
#define TEMPERATURE_SAMPLE_RATE_HZ 10      // 10Hz temperature

/* Sensor Buffers */
#define VIBRATION_BUFFER_SIZE      128     // 128 samples for FFT
#define FFT_SIZE                   128     // FFT size (must be power of 2)
#define FFT_BINS                   (FFT_SIZE / 2)

/* Sensor Pins (Platform Specific) */
#ifdef SIMULATION_MODE
    #define VIBRATION_PIN          0        // Simulated
    #define TEMPERATURE_PIN        1        // Simulated
    #define EMERGENCY_BUTTON_PIN   2        // Simulated
#else
    /* AmebaPro2 GPIO Pins */
    #define VIBRATION_PIN          PA_0     // Analog input
    #define TEMPERATURE_PIN        PA_1     // I2C data
    #define EMERGENCY_BUTTON_PIN   PB_2     // Digital input with interrupt
    #define SPI_MOSI_PIN          PC_2
    #define SPI_MISO_PIN          PC_3
    #define SPI_CLK_PIN           PC_1
    #define SPI_CS_PIN            PC_0
#endif

/*-----------------------------------------------------------
 * AI/ML Configuration
 *----------------------------------------------------------*/

/* Autoencoder Model Parameters */
#define INPUT_FEATURES             10      // Input dimension
#define HIDDEN_LAYER1_SIZE         8       // First hidden layer
#define LATENT_SPACE_SIZE          4       // Bottleneck layer
#define HIDDEN_LAYER2_SIZE         8       // Second hidden layer
#define OUTPUT_FEATURES            10      // Output dimension (same as input)

/* Inference Settings */
#define INFERENCE_BATCH_SIZE       5       // Process 5 samples at once
#define SLIDING_WINDOW_SIZE        10      // Historical samples for temporal analysis
#define ANOMALY_THRESHOLD_BASE     0.15    // Base MSE threshold
#define ANOMALY_THRESHOLD_SCALE    3.0     // Scale factor (3-sigma)

/* Quantization */
#define QUANTIZATION_BITS          8       // INT8 quantization
#define QUANTIZATION_SCALE         127.0f  // Scale factor for INT8

/*-----------------------------------------------------------
 * Network Configuration
 *----------------------------------------------------------*/

/* WiFi Settings */
#define WIFI_SSID                  "WindTurbineNet"          // Example SSID - change for production
#define WIFI_PASSWORD              "SecurePassword123"       // Placeholder password - change for production
#define WIFI_RECONNECT_DELAY_MS    5000    // 5 seconds between reconnect attempts
#define WIFI_MAX_RETRY             3       // Max connection retries

/* Cloud Endpoint */
#define CLOUD_SERVER_URL           "https://turbine-monitor.example.com"
#define CLOUD_SERVER_PORT          8883    // MQTT over TLS
#define DEVICE_ID                  "TURBINE_001"
#define API_KEY                    "YOUR_API_KEY_HERE"       // Placeholder API key - set real value in production

/* Network Buffers */
#define NETWORK_BUFFER_SIZE        1024    // 1KB network buffer
#define MAX_PACKET_SIZE            512     // Max packet size

/*-----------------------------------------------------------
 * Power Management (Capability 8)
 *----------------------------------------------------------*/

/* Sleep Configuration */
#define MIN_IDLE_TIME_FOR_SLEEP_MS 1000   // Min 1 second idle before deep sleep
#define WAKE_SOURCES               (WAKE_SOURCE_RTC | WAKE_SOURCE_GPIO)
#define BATTERY_LOW_THRESHOLD      20      // 20% battery threshold
#define BATTERY_CRITICAL_THRESHOLD 5       // 5% critical battery

/* Power Modes */
typedef enum {
    POWER_MODE_ACTIVE = 0,     // Full operation - 50mA
    POWER_MODE_IDLE,          // CPU idle - 20mA
    POWER_MODE_STANDBY,       // Peripherals off - 5mA
    POWER_MODE_DEEP_SLEEP     // RTC only - 50μA
} power_mode_t;

/*-----------------------------------------------------------
 * Safety and Thresholds
 *----------------------------------------------------------*/

/* Safety Thresholds */
#define MAX_BLADE_RPM              30      // Maximum safe RPM
#define MAX_BEARING_TEMP_C         85      // Maximum bearing temperature
#define MAX_VIBRATION_G            5.0     // Maximum vibration in G
#define EMERGENCY_STOP_DELAY_MS    100     // Delay before emergency stop

/* Anomaly Severity Levels */
typedef enum {
    SEVERITY_NONE = 0,
    SEVERITY_LOW,          // Monitor closely
    SEVERITY_MEDIUM,       // Schedule maintenance
    SEVERITY_HIGH,         // Immediate inspection
    SEVERITY_CRITICAL      // Emergency stop
} anomaly_severity_t;

/*-----------------------------------------------------------
 * Memory Configuration (Capability 6)
 *----------------------------------------------------------*/

/* Heap Monitoring */
#define HEAP_WARNING_THRESHOLD     0.8     // Warn at 80% heap usage
#define HEAP_CRITICAL_THRESHOLD    0.95    // Critical at 95% heap usage
#define HEAP_CHECK_PERIOD_MS       10000   // Check heap every 10 seconds

/* Memory Pools (Static Allocation) */
#define MAX_SENSOR_BUFFERS         5       // Pre-allocated sensor buffers
#define MAX_NETWORK_BUFFERS        3       // Pre-allocated network buffers
#define EMERGENCY_BUFFER_SIZE      1024    // Emergency buffer for critical ops

/*-----------------------------------------------------------
 * Debug and Logging
 *----------------------------------------------------------*/

/* Debug Levels */
#define DEBUG_LEVEL_NONE           0
#define DEBUG_LEVEL_ERROR          1
#define DEBUG_LEVEL_WARNING        2
#define DEBUG_LEVEL_INFO           3
#define DEBUG_LEVEL_DEBUG          4
#define DEBUG_LEVEL_VERBOSE        5

/* Current Debug Level */
#ifdef DEBUG_BUILD
    #define CURRENT_DEBUG_LEVEL    DEBUG_LEVEL_DEBUG
#else
    #define CURRENT_DEBUG_LEVEL    DEBUG_LEVEL_ERROR
#endif

/* Log Configuration */
#define LOG_BUFFER_SIZE            256     // Log message buffer
#define MAX_LOG_FILES              10      // Rotate after 10 files
#define LOG_FILE_MAX_SIZE          (1024 * 1024)  // 1MB per log file

/*-----------------------------------------------------------
 * System Configuration
 *----------------------------------------------------------*/

/* Watchdog */
#define WATCHDOG_TIMEOUT_MS        5000    // 5 second watchdog timeout
#define WATCHDOG_FEED_PERIOD_MS    1000    // Feed watchdog every second

/* System Timing */
#define SYSTEM_TICK_MS             1       // 1ms system tick
#define STARTUP_DELAY_MS           1000    // 1 second startup delay

/* Version Information */
#define FIRMWARE_VERSION_MAJOR     1
#define FIRMWARE_VERSION_MINOR     0
#define FIRMWARE_VERSION_PATCH     0
#define FIRMWARE_VERSION_STRING    "1.0.0"

/* Feature Flags */
#define ENABLE_AI_INFERENCE        1       // Enable AI anomaly detection
#define ENABLE_CLOUD_SYNC         1       // Enable cloud synchronization
#define ENABLE_SD_LOGGING         1       // Enable SD card logging
#define ENABLE_POWER_MANAGEMENT   1       // Enable power saving features
#define ENABLE_WATCHDOG           1       // Enable watchdog timer
#define ENABLE_STACK_MONITORING   1       // Enable stack usage monitoring

#endif /* APP_CONFIG_H */
