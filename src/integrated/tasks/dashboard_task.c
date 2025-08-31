/**
 * Dashboard Task - Real-time console visualization
 * Priority: 1 (Lowest)
 * Frequency: 10Hz (100ms refresh)
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "../common/system_state.h"
#include "../dashboard/console.h"

// Dashboard parameters
#define DASHBOARD_REFRESH_MS    1000  // 1Hz refresh rate - slower to observe task switching
#define CLEAR_INTERVAL         5      // Clear screen every 5 seconds (5 * 1000ms)

// External references
extern SystemState_t g_system_state;

// Proactive Stack Health Checking Function (Capability 7)
// This demonstrates good coding practice: check before problems occur!
static void check_stack_health(void) {
    StackMonitoringSystem_t* stack_sys = &g_system_state.stack_monitoring;
    TickType_t current_time = xTaskGetTickCount();
    bool found_issues = false;
    
    // Check current task's own stack first (self-monitoring)
    UBaseType_t my_stack_free = uxTaskGetStackHighWaterMark(NULL);
    if (my_stack_free < 100) {  // Less than 100 words free is concerning for dashboard task
        printf("[STACK HEALTH] WARNING: Dashboard task low stack! Free: %d words\n", (int)my_stack_free);
        found_issues = true;
    }
    
    // Check all monitored tasks for concerning trends
    for (uint32_t i = 0; i < stack_sys->monitored_count; i++) {
        TaskStackMonitor_t* task = &stack_sys->tasks[i];
        
        // Skip system tasks for this check
        if (strstr(task->task_name, "IDLE") || strstr(task->task_name, "Tmr")) {
            continue;
        }
        
        // Check for high usage without warnings yet (edge case)
        if (task->usage_percent >= 65 && task->usage_percent < 70 && !task->warning_issued) {
            printf("[STACK HEALTH] INFO: Task %s approaching 70%% threshold (%d%% used)\n", 
                   task->task_name, task->usage_percent);
        }
        
        // Check for tasks that haven't been updated recently (possible crash)
        uint32_t time_since_check = current_time - task->last_check_time;
        if (time_since_check > pdMS_TO_TICKS(5000)) {  // 5 seconds
            printf("[STACK HEALTH] WARNING: Task %s not checked recently (%ds ago)\n", 
                   task->task_name, (int)(time_since_check / configTICK_RATE_HZ));
            found_issues = true;
        }
    }
    
    // Demonstrate good coding practices with comments
    if (!found_issues && stack_sys->global_stats.proactive_checks % 100 == 0) {
        printf("[STACK HEALTH] Good practice: %lu proactive checks performed, no issues found\n", 
               (unsigned long)stack_sys->global_stats.proactive_checks);
    }
    
    // Example of what NOT to do (bad practice demonstration)
    // Uncomment these lines to see stack overflow in action:
    /*
    // BAD PRACTICE: Large local arrays
    // char bad_large_array[5000];  // This would consume 5KB of stack!
    // printf("Bad practice: Large array at %p\n", bad_large_array);
    
    // BAD PRACTICE: Deep recursion
    // recursive_function(100);  // This would overflow the stack
    */
}

void vDashboardTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DASHBOARD_REFRESH_MS);
    
    uint32_t cycle_count = 0;
    
    // Initial clear
    console_clear();
    
    while (1) {
        // Wait for the next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        cycle_count++;
        
        // Check if dashboard is enabled
        if (!g_system_state.dashboard_enabled) {
            vTaskDelay(pdMS_TO_TICKS(1000));  // Sleep longer if disabled
            continue;
        }
        
        // Clear screen periodically for clean display
        if (cycle_count % CLEAR_INTERVAL == 0) {
            console_clear();
        }
        
        // Draw the dashboard
        console_draw_dashboard();
        
        // Proactive Stack Health Check (Capability 7) - Good coding practice!
        if (cycle_count % 10 == 0) {  // Check every 10 cycles
            check_stack_health();
        }
        
        // Power Management Awareness (Capability 8) - Adaptive refresh rate
        if (g_system_state.power_stats.power_savings_percent > 50) {
            // Reduce dashboard refresh rate when power saving is active
            vTaskDelay(pdMS_TO_TICKS(1000));  // Additional 1s delay for power savings
        }
        
        // This low-priority task gets preempted frequently
        // It's okay because UI updates are not critical
        
        // Update context switch count
        // g_system_state.context_switch_count++; // Now tracked in update_task_stats()
        
        // Always yield to higher priority tasks
        taskYIELD();
    }
}
