/**
 * Console Dashboard Rendering
 * ANSI escape codes for terminal UI
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "../common/system_state.h"
#include "console.h"

// ANSI escape codes
#define CLEAR_SCREEN    "\033[2J\033[H"
#define CURSOR_HOME     "\033[H"
#define BOLD            "\033[1m"
#define NORMAL          "\033[0m"
#define RED             "\033[31m"
#define GREEN           "\033[32m"
#define YELLOW          "\033[33m"
#define BLUE            "\033[34m"
#define CYAN            "\033[36m"
#define WHITE           "\033[37m"
#define BG_RED          "\033[41m"
#define BG_GREEN        "\033[42m"
#define BG_YELLOW       "\033[43m"

// Progress bar characters
#define BLOCK_FULL      "#"
#define BLOCK_EMPTY     "-"

// External references
extern SystemState_t g_system_state;
extern void update_task_stats(void);
extern QueueHandle_t xSensorDataQueue;
extern QueueHandle_t xAnomalyAlertQueue;

// Draw progress bar
static void draw_progress_bar(float percentage, int width) {
    int filled = (int)((percentage / 100.0) * width);
    printf("[");
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            printf(BLOCK_FULL);
        } else {
            printf(BLOCK_EMPTY);
        }
    }
    printf("]");
}

// Get color for value based on thresholds
static const char* get_status_color(float value, float warning, float critical) {
    if (value >= critical) return RED;
    if (value >= warning) return YELLOW;
    return GREEN;
}

// Format uptime
static void format_uptime(uint32_t seconds, char* buffer) {
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    sprintf(buffer, "%02d:%02d:%02d", hours, minutes, secs);
}

// Clear and reset console
void console_clear(void) {
    printf(CLEAR_SCREEN);
}

// Draw the main dashboard
void console_draw_dashboard(void) {
    char uptime_str[32];
    
    // Update statistics
    update_task_stats();
    g_system_state.uptime_seconds = xTaskGetTickCount() / configTICK_RATE_HZ;
    format_uptime(g_system_state.uptime_seconds, uptime_str);
    
    // Clear and home cursor
    printf(CURSOR_HOME);
    
    // Header
    printf(BOLD "==========================================================\n" NORMAL);
    printf(BOLD " WIND TURBINE MONITOR - CAPABILITIES 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8\n" NORMAL);
    printf(BOLD "                    " YELLOW "[SIMULATION MODE]" NORMAL "\n");
    printf(BOLD "==========================================================\n\n" NORMAL);
    
    // Task Scheduler Status
    printf(BOLD "TASK SCHEDULER STATUS" NORMAL "                   Tick: %lu\n", xTaskGetTickCount());
    printf("----------------------------------------------------------\n");
    printf(BOLD "RUNNING TASKS (%d active):\n" NORMAL, g_system_state.task_count);
    
    // List tasks with their status
    for (uint32_t i = 0; i < g_system_state.task_count && i < 6; i++) {
        TaskStats_t* task = &g_system_state.tasks[i];
        
        // Skip IDLE and Timer tasks
        if (strstr(task->name, "IDLE") || strstr(task->name, "Tmr")) {
            continue;
        }
        
        // Task state color
        const char* state_color = GREEN;
        if (task->state == eBlocked) state_color = YELLOW;
        else if (task->state == eSuspended) state_color = RED;
        
        printf("  [%d] %-15s %s%-10s" NORMAL " CPU:%3d%%*  Stack:%3d%%*\n",
               task->priority,
               task->name,
               state_color,
               task_state_to_string(task->state),
               task->cpu_usage_percent,
               task->stack_usage_percent);
    }
    
    printf("\n");
    
    // Sensor Readings
    printf(BOLD "SENSOR READINGS:\n" NORMAL);
    
    // Vibration
    printf("  Vibration: %s%.2f mm/s" NORMAL "   ", 
           get_status_color(g_system_state.sensors.vibration, 5.0, 10.0),
           g_system_state.sensors.vibration);
    
    // Temperature  
    printf("Temperature: %s%.1f°C" NORMAL "    ",
           get_status_color(g_system_state.sensors.temperature, 70.0, 85.0),
           g_system_state.sensors.temperature);
    
    // RPM
    printf("RPM: %s%.1f" NORMAL "\n",
           g_system_state.sensors.rpm < 10.0 || g_system_state.sensors.rpm > 30.0 ? YELLOW : GREEN,
           g_system_state.sensors.rpm);
    
    // ISR Status (Capability 2)
    printf("\n" BOLD "ISR STATUS:\n" NORMAL);
    printf("  Active | Rate: 100Hz | Latency: %luµs | Count: %lu/%lu\n",
           g_system_state.isr_stats.last_latency_us,
           g_system_state.isr_stats.processed_count,
           g_system_state.isr_stats.interrupt_count);
    
    // Queue Status (Capability 3)
    UBaseType_t sensor_queue_count = uxQueueMessagesWaiting(xSensorDataQueue);
    UBaseType_t anomaly_queue_count = uxQueueMessagesWaiting(xAnomalyAlertQueue);
    printf("\n" BOLD "QUEUE STATUS:\n" NORMAL);
    printf("  Sensor[%lu/5] Anomaly[%lu/3] (Used/Size)\n",
           (unsigned long)sensor_queue_count,
           (unsigned long)anomaly_queue_count);
    
    // Mutex Status (Capability 4)
    printf("\n" BOLD "MUTEX STATUS:\n" NORMAL);
    printf("  System State: Takes:%lu Gives:%lu Timeouts:%lu\n",
           (unsigned long)g_system_state.mutex_stats.system_mutex_takes,
           (unsigned long)g_system_state.mutex_stats.system_mutex_gives,
           (unsigned long)g_system_state.mutex_stats.system_mutex_timeouts);
    printf("  Thresholds:   Takes:%lu Gives:%lu Timeouts:%lu\n",
           (unsigned long)g_system_state.mutex_stats.threshold_mutex_takes,
           (unsigned long)g_system_state.mutex_stats.threshold_mutex_gives,
           (unsigned long)g_system_state.mutex_stats.threshold_mutex_timeouts);
    
    printf("\n");
    
    // Event Group Status (Capability 5)
    printf(BOLD "EVENT GROUP STATUS:\n" NORMAL);
    
    // Show individual system status
    const char* sensors_status = (g_system_state.event_group_stats.current_event_bits & 0x01) ? GREEN "[✓] Sensors" NORMAL : RED "[ ] Sensors" NORMAL;
    const char* network_status = (g_system_state.event_group_stats.current_event_bits & 0x02) ? GREEN "[✓] Network" NORMAL : RED "[ ] Network" NORMAL;
    const char* anomaly_status = (g_system_state.event_group_stats.current_event_bits & 0x04) ? GREEN "[✓] Anomaly" NORMAL : RED "[ ] Anomaly" NORMAL;
    
    // Calculate ready time
    float ready_time_sec = 0.0f;
    if (g_system_state.event_group_stats.system_ready_time > 0) {
        ready_time_sec = (float)g_system_state.event_group_stats.system_ready_time / (float)configTICK_RATE_HZ;
    }
    
    // Overall status
    const char* overall_status = ((g_system_state.event_group_stats.current_event_bits & 0x07) == 0x07) ? 
        GREEN "ALL READY" NORMAL : YELLOW "WAITING" NORMAL;
    
    printf("  System Ready: %s %s %s → %s", sensors_status, network_status, anomaly_status, overall_status);
    if (ready_time_sec > 0) {
        printf(" (%.1fs)\n", ready_time_sec);
    } else {
        printf("\n");
    }
    
    printf("  Operations: Sets:%lu Clears:%lu Waits:%lu\n",
           (unsigned long)g_system_state.event_group_stats.bits_set_count,
           (unsigned long)g_system_state.event_group_stats.bits_cleared_count,
           (unsigned long)g_system_state.event_group_stats.wait_operations);
    
    printf("\n");
    
    // Memory Management Status (Capability 6)
    printf(BOLD "MEMORY STATUS:\n" NORMAL);
    
    // Calculate memory usage percentage using real-time heap info
    size_t total_heap = configTOTAL_HEAP_SIZE;
    size_t current_free = xPortGetFreeHeapSize();  // Get current free heap directly
    size_t used_heap = total_heap - current_free;
    float heap_usage_percent = (float)used_heap / (float)total_heap * 100.0f;
    
    // Calculate fragmentation estimate based on active allocations vs used space
    float fragmentation_percent = 0.0f;
    if (g_system_state.memory_stats.active_allocations > 0 && used_heap > 0) {
        // Estimate fragmentation as percentage of unused space in allocated regions
        fragmentation_percent = ((float)g_system_state.memory_stats.active_allocations / 20.0f) * 
                               (heap_usage_percent / 100.0f) * 100.0f;
        if (fragmentation_percent > 20.0f) fragmentation_percent = 20.0f;  // Cap at reasonable value
    }
    
    printf("  Heap Usage: %zu/%zu bytes (%.1f%%) | Peak: %zu bytes\n",
           used_heap, total_heap, heap_usage_percent, g_system_state.memory_stats.peak_usage);
    
    printf("  Active Allocs: %lu | Total: Allocs:%lu Frees:%lu Fails:%lu\n",
           (unsigned long)g_system_state.memory_stats.active_allocations,
           (unsigned long)g_system_state.memory_stats.allocations,
           (unsigned long)g_system_state.memory_stats.deallocations,
           (unsigned long)g_system_state.memory_stats.allocation_failures);
    
    printf("  Fragmentation: %.1f%% | Min Free: %zu bytes\n",
           fragmentation_percent, g_system_state.memory_stats.minimum_heap_free);
    
    printf("\n");
    
    // Stack Status (Capability 7)
    printf(BOLD "STACK STATUS:\n" NORMAL);
    
    StackMonitoringSystem_t* stack_sys = &g_system_state.stack_monitoring;
    printf("  Monitored Tasks: %lu | Warnings: %lu | Critical: %lu | Overflows: %lu\n",
           (unsigned long)stack_sys->global_stats.tasks_monitored,
           (unsigned long)stack_sys->global_stats.warnings_issued,
           (unsigned long)stack_sys->global_stats.critical_usage_events,
           (unsigned long)stack_sys->global_stats.overflow_events);
    
    // Show individual task stack usage
    printf("  Task Stack Usage:\n");
    for (uint32_t i = 0; i < stack_sys->monitored_count && i < 5; i++) {
        TaskStackMonitor_t* task = &stack_sys->tasks[i];
        
        // Skip IDLE and Timer tasks for cleaner display
        if (strstr(task->task_name, "IDLE") || strstr(task->task_name, "Tmr")) {
            continue;
        }
        
        // Color code based on usage level
        const char* color = GREEN;
        if (task->usage_percent >= 85) color = RED;
        else if (task->usage_percent >= 70) color = YELLOW;
        
        printf("    %-12s %s%3d%%" NORMAL " (Peak: %d%%, Free: %d words)\n",
               task->task_name, color, task->usage_percent, 
               task->peak_usage_percent, (int)task->current_high_water);
    }
    
    // Show last warning if any
    if (stack_sys->global_stats.warnings_issued > 0) {
        uint32_t warning_age_sec = (xTaskGetTickCount() - stack_sys->global_stats.last_warning_time) / configTICK_RATE_HZ;
        printf("  Last Warning: %s (%ds ago)\n", 
               stack_sys->global_stats.last_warning_task, warning_age_sec);
    }
    
    printf("  Proactive Checks: %lu (Good coding practice!)\n", 
           (unsigned long)stack_sys->global_stats.proactive_checks);
    
    printf("\n");
    
    // Power Status (Capability 8)
    printf(BOLD "POWER STATUS:\n" NORMAL);
    printf("  Idle Entries: %lu | Sleep Entries: %lu | Wake Events: %lu\n",
           (unsigned long)g_system_state.power_stats.idle_entries,
           (unsigned long)g_system_state.power_stats.sleep_entries,
           (unsigned long)g_system_state.power_stats.wake_events);

    printf("  Power Savings: %lu%% | Total Sleep: %lu ms | Last Wake: %s\n",
           (unsigned long)g_system_state.power_stats.power_savings_percent,
           (unsigned long)g_system_state.power_stats.total_sleep_time_ms,
           g_system_state.power_stats.last_wake_source);
    
    printf("\n");
    
    // Preemption Events
    printf(BOLD "PREEMPTION EVENTS (Last 5):\n" NORMAL);
    uint32_t start_idx = g_system_state.preemption_index > 5 ? 
                        g_system_state.preemption_index - 5 : 0;
    for (uint32_t i = start_idx; i < g_system_state.preemption_index && 
                                  i < start_idx + 5; i++) {
        PreemptionEvent_t* event = &g_system_state.preemption_history[i % PREEMPTION_HISTORY_SIZE];
        if (event->tick > 0) {
            printf("  [%6lu] %-15.15s preempted %-15.15s (%-10.10s)\n",
                   event->tick,
                   event->preemptor,
                   event->preempted,
                   event->reason ? event->reason : "Unknown");
        }
    }
    
    printf("\n");
    
    // Scheduling Metrics
    printf(BOLD "SCHEDULING METRICS:\n" NORMAL);
    printf("  Context Switches: %-10lu* Idle Time: %d%%*\n",
           g_system_state.context_switch_count,
           g_system_state.idle_time_percent);
    printf("  Task Switches/sec: %-9lu* CPU Usage: %d%%*\n",
           g_system_state.context_switch_count / (g_system_state.uptime_seconds + 1),
           g_system_state.cpu_usage_percent);
    
    // Health Status Bar
    printf("\n" BOLD "HEALTH STATUS: " NORMAL);
    float health = g_system_state.anomalies.health_score;
    const char* health_color = health > 80 ? GREEN : (health > 50 ? YELLOW : RED);
    const char* health_text = health > 80 ? "HEALTHY" : (health > 50 ? "WARNING" : "CRITICAL");
    
    printf("%s%.0f%%" NORMAL " ", health_color, health);
    draw_progress_bar(health, 20);
    printf(" %s%s" NORMAL "\n", health_color, health_text);
    
    // Emergency Stop Warning
    if (g_system_state.emergency_stop) {
        printf("\n" BG_RED BOLD " EMERGENCY STOP ACTIVE " NORMAL "\n");
    }
    
    // Footer
    printf("\n----------------------------------------------------------\n");
    printf("Uptime: %s | Network: %s | Anomalies: %lu\n",
           uptime_str,
           g_system_state.network_connected ? GREEN "Connected" NORMAL : RED "Disconnected" NORMAL,
           g_system_state.anomalies.anomaly_count);
    printf(CYAN "* Estimated metrics (POSIX simulation)" NORMAL "\n");
    printf("Press Ctrl+C to exit\n");
}