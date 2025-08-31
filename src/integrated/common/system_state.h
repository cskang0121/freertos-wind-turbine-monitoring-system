#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

// System Constants
#define MAX_TASK_NAME_LEN 16
#define MAX_TASKS_TRACKED 10
#define PREEMPTION_HISTORY_SIZE 10

// Sensor Data Structure
typedef struct {
    float vibration;      // mm/s
    float temperature;    // Celsius
    float rpm;           // Rotations per minute
    float current;       // Amps
    uint32_t timestamp;  // System ticks
} SensorData_t;

// Task Statistics
typedef struct {
    char name[MAX_TASK_NAME_LEN];
    UBaseType_t priority;
    eTaskState state;
    uint32_t cpu_usage_percent;
    uint32_t stack_usage_percent;
    uint32_t runtime;
    uint32_t prev_runtime;  // For delta calculation
    uint32_t context_switches;
} TaskStats_t;

// Preemption Event
typedef struct {
    TickType_t tick;
    char preemptor[MAX_TASK_NAME_LEN];
    char preempted[MAX_TASK_NAME_LEN];
    const char* reason;  // "Priority", "Yield", "Block"
} PreemptionEvent_t;

// Anomaly Detection Results
typedef struct {
    bool vibration_anomaly;
    bool temperature_anomaly;
    bool rpm_anomaly;
    float health_score;      // 0-100%
    uint32_t anomaly_count;
} AnomalyResults_t;

// ISR Statistics (Capability 2)
typedef struct {
    uint32_t interrupt_count;
    uint32_t processed_count;
    uint32_t last_latency_us;
} ISRStats_t;

// Mutex Statistics (Capability 4)
typedef struct {
    uint32_t system_mutex_takes;
    uint32_t system_mutex_gives;
    uint32_t system_mutex_timeouts;
    uint32_t threshold_mutex_takes;
    uint32_t threshold_mutex_gives;
    uint32_t threshold_mutex_timeouts;
} MutexStats_t;

// Event Group Statistics (Capability 5)
typedef struct {
    uint32_t bits_set_count;        // Number of times bits were set
    uint32_t bits_cleared_count;    // Number of times bits were cleared
    uint32_t wait_operations;       // Number of wait operations
    uint32_t current_event_bits;    // Current state of event bits
    TickType_t system_ready_time;   // When all systems became ready (0 = not ready yet)
} EventGroupStats_t;

// Memory Management Statistics (Capability 6)
typedef struct {
    uint32_t allocations;           // Total number of allocations
    uint32_t deallocations;         // Total number of deallocations
    uint32_t allocation_failures;   // Failed allocation attempts
    size_t bytes_allocated;         // Total bytes currently allocated
    size_t peak_usage;              // Peak memory usage
    size_t current_heap_free;       // Current free heap size
    size_t minimum_heap_free;       // Minimum free heap ever seen
    uint32_t active_allocations;    // Currently active allocations
} MemoryStats_t;

// Stack Overflow Detection Statistics (Capability 7)
typedef struct {
    uint32_t warnings_issued;          // Number of stack warnings issued
    uint32_t high_usage_events;        // Times stack exceeded 70%
    uint32_t critical_usage_events;    // Times stack exceeded 85%
    uint32_t overflow_events;          // Stack overflow occurrences
    uint32_t proactive_checks;         // Number of proactive checks performed
    uint32_t tasks_monitored;          // Number of tasks being monitored
    TickType_t last_warning_time;      // When last warning was issued
    char last_warning_task[MAX_TASK_NAME_LEN];  // Task that triggered last warning
} StackStats_t;

// Per-task stack monitoring
typedef struct {
    char task_name[MAX_TASK_NAME_LEN];
    UBaseType_t stack_size_words;      // Configured stack size in words
    UBaseType_t current_high_water;    // Current free stack (words)
    UBaseType_t minimum_high_water;    // Minimum free stack ever seen
    uint32_t usage_percent;            // Current usage percentage
    uint32_t peak_usage_percent;       // Peak usage percentage
    bool warning_issued;               // Warning flag for this task
    TickType_t last_check_time;        // When last checked
} TaskStackMonitor_t;

#define MAX_STACK_MONITORED_TASKS 8
typedef struct {
    TaskStackMonitor_t tasks[MAX_STACK_MONITORED_TASKS];
    uint32_t monitored_count;
    StackStats_t global_stats;
} StackMonitoringSystem_t;

// Power Management Statistics (Capability 8)
typedef struct {
    uint32_t idle_entries;              // Number of times entered idle
    uint32_t sleep_entries;             // Number of times entered tickless sleep
    uint32_t total_sleep_time_ms;       // Total time spent in sleep (ms)
    uint32_t power_savings_percent;     // Estimated power savings
    uint32_t wake_events;               // Number of wake events
    char last_wake_source[16];          // Last wake source description
} PowerStats_t;

// ISR Sensor Data
typedef struct {
    float vibration;
    TickType_t timestamp;
    uint32_t sequence;
} SensorISRData_t;

// Queue Communication Structures (Capability 3)
typedef struct {
    float severity;      // 0-10 scale
    uint32_t type;      // 0=vibration, 1=temperature, 2=rpm
    TickType_t timestamp;
} AnomalyAlert_t;

// System State (Shared across all tasks)
typedef struct {
    // Sensor readings
    SensorData_t sensors;
    
    // Anomaly detection
    AnomalyResults_t anomalies;
    
    // Task scheduling metrics
    TaskStats_t tasks[MAX_TASKS_TRACKED];
    uint32_t task_count;
    uint32_t context_switch_count;
    uint32_t idle_time_percent;
    
    // Preemption tracking
    PreemptionEvent_t preemption_history[PREEMPTION_HISTORY_SIZE];
    uint32_t preemption_index;
    
    // ISR metrics (Capability 2)
    ISRStats_t isr_stats;
    
    // Mutex metrics (Capability 4)
    MutexStats_t mutex_stats;
    
    // Event Group metrics (Capability 5)
    EventGroupStats_t event_group_stats;
    
    // Memory Management metrics (Capability 6)
    MemoryStats_t memory_stats;
    
    // Stack Overflow Detection metrics (Capability 7)
    StackMonitoringSystem_t stack_monitoring;
    
    // Power Management metrics (Capability 8)
    PowerStats_t power_stats;
    
    // System metrics
    uint32_t uptime_seconds;
    uint32_t cpu_usage_percent;
    bool emergency_stop;
    bool network_connected;
    
    // Dashboard control
    bool dashboard_enabled;
    uint32_t refresh_rate_ms;
} SystemState_t;

// Global system state (extern declaration)
extern SystemState_t g_system_state;

// Threshold Configuration
typedef struct {
    float vibration_warning;     // mm/s
    float vibration_critical;    // mm/s
    float temperature_warning;   // Celsius
    float temperature_critical;  // Celsius
    float rpm_min;              // RPM
    float rpm_max;              // RPM
    float current_max;          // Amps
} ThresholdConfig_t;

extern ThresholdConfig_t g_thresholds;

// Utility functions
void system_state_init(void);
void record_preemption(const char* preemptor, const char* preempted, const char* reason);
void update_task_stats(void);
const char* task_state_to_string(eTaskState state);

#endif // SYSTEM_STATE_H
