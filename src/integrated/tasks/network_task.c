/**
 * Network Task - Simulates data transmission to cloud
 * Priority: 2 (Low)
 * Frequency: 1Hz
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "../common/system_state.h"

// Network parameters
#define NETWORK_SEND_RATE_MS    1000  // 1Hz
#define PACKET_SIZE            256
#define TRANSMISSION_TIME_MS    50    // Simulated network latency

// External references
extern SystemState_t g_system_state;
extern void record_preemption(const char* preemptor, const char* preempted, const char* reason);
extern QueueHandle_t xAnomalyAlertQueue;  // Capability 3: Receive anomaly alerts
extern SemaphoreHandle_t xSystemStateMutex;  // Capability 4: Mutex protection
extern EventGroupHandle_t xSystemReadyEvents;  // Capability 5: Event groups

// Event bits (defined in main.c)
#define NETWORK_CONNECTED_BIT   (1 << 1)  // 0x02 - NetworkTask connected

// Dynamic packet sizes (Capability 6: Memory Management)
#define PACKET_HEARTBEAT_SIZE   64   // Small heartbeat packets
#define PACKET_SENSOR_SIZE      256  // Medium sensor data packets  
#define PACKET_ANOMALY_SIZE     512  // Large anomaly report packets

// Packet types
typedef enum {
    PACKET_TYPE_HEARTBEAT,
    PACKET_TYPE_SENSOR_DATA,
    PACKET_TYPE_ANOMALY_REPORT
} PacketType_t;

// Dynamic packet buffer structure
typedef struct {
    PacketType_t type;
    uint32_t size;
    uint32_t timestamp;
    char data[];  // Flexible array member
} PacketBuffer_t;

// Network statistics
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_failed;
    uint32_t bytes_sent;
    uint32_t anomaly_alerts_sent;
    uint32_t last_transmission_time;
    bool transmission_in_progress;
} NetworkStats_t;

static NetworkStats_t network_stats = {0};

// Memory tracking helper functions (Capability 6)
static void update_memory_stats_alloc(size_t size) {
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        g_system_state.memory_stats.allocations++;
        g_system_state.memory_stats.active_allocations++;
        g_system_state.memory_stats.bytes_allocated += size;
        
        // Update peak usage
        if (g_system_state.memory_stats.bytes_allocated > g_system_state.memory_stats.peak_usage) {
            g_system_state.memory_stats.peak_usage = g_system_state.memory_stats.bytes_allocated;
        }
        
        // Update current heap free and track minimum
        size_t current_free = xPortGetFreeHeapSize();
        g_system_state.memory_stats.current_heap_free = current_free;
        
        // Bounds checking and debug logging for minimum heap tracking
        if (current_free < g_system_state.memory_stats.minimum_heap_free) {
            size_t old_min = g_system_state.memory_stats.minimum_heap_free;
            g_system_state.memory_stats.minimum_heap_free = current_free;
            printf("[MEMORY DEBUG] Min heap updated from %zu to %zu bytes (alloc)\n", old_min, current_free);
        }
        
        // Prevent minimum from being set to 0 unless heap is actually exhausted
        if (g_system_state.memory_stats.minimum_heap_free == 0 && current_free > 0) {
            g_system_state.memory_stats.minimum_heap_free = current_free;
            printf("[MEMORY DEBUG] Fixed minimum heap from 0 to %zu bytes\n", current_free);
        }
        
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
}

static void update_memory_stats_free(size_t size) {
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        g_system_state.memory_stats.deallocations++;
        g_system_state.memory_stats.active_allocations--;
        g_system_state.memory_stats.bytes_allocated -= size;
        
        // Update current heap free and track minimum
        size_t current_free = xPortGetFreeHeapSize();
        g_system_state.memory_stats.current_heap_free = current_free;
        
        // Track minimum heap during deallocation (should usually not decrease minimum)
        if (current_free < g_system_state.memory_stats.minimum_heap_free) {
            size_t old_min = g_system_state.memory_stats.minimum_heap_free;
            g_system_state.memory_stats.minimum_heap_free = current_free;
            printf("[MEMORY DEBUG] Min heap updated from %zu to %zu bytes (free)\n", old_min, current_free);
        }
        
        // Prevent minimum from being set to 0 unless heap is actually exhausted
        if (g_system_state.memory_stats.minimum_heap_free == 0 && current_free > 0) {
            g_system_state.memory_stats.minimum_heap_free = current_free;
            printf("[MEMORY DEBUG] Fixed minimum heap from 0 to %zu bytes (free)\n", current_free);
        }
        
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
}

static void update_memory_stats_failure() {
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        g_system_state.memory_stats.allocation_failures++;
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
}

// Dynamic packet allocation functions (Capability 6)
static PacketBuffer_t* allocate_packet(PacketType_t type) {
    size_t packet_size;
    
    // Determine packet size based on type
    switch (type) {
        case PACKET_TYPE_HEARTBEAT:
            packet_size = sizeof(PacketBuffer_t) + PACKET_HEARTBEAT_SIZE;
            break;
        case PACKET_TYPE_SENSOR_DATA:
            packet_size = sizeof(PacketBuffer_t) + PACKET_SENSOR_SIZE;
            break;
        case PACKET_TYPE_ANOMALY_REPORT:
            packet_size = sizeof(PacketBuffer_t) + PACKET_ANOMALY_SIZE;
            break;
        default:
            packet_size = sizeof(PacketBuffer_t) + PACKET_SENSOR_SIZE;
            break;
    }
    
    // Allocate memory for packet
    PacketBuffer_t* packet = (PacketBuffer_t*)pvPortMalloc(packet_size);
    
    if (packet != NULL) {
        packet->type = type;
        packet->size = packet_size;
        packet->timestamp = xTaskGetTickCount();
        update_memory_stats_alloc(packet_size);
    } else {
        update_memory_stats_failure();
    }
    
    return packet;
}

static void free_packet(PacketBuffer_t* packet) {
    if (packet != NULL) {
        size_t packet_size = packet->size;
        vPortFree(packet);
        update_memory_stats_free(packet_size);
    }
}

// Simulate network packet creation
static uint32_t create_packet(char* buffer, uint32_t max_size) {
    // Create JSON-like packet
    int len = snprintf(buffer, max_size,
        "{"
        "\"timestamp\":%u,"
        "\"vibration\":%.2f,"
        "\"temperature\":%.2f,"
        "\"rpm\":%.2f,"
        "\"current\":%.2f,"
        "\"health_score\":%.1f,"
        "\"anomalies\":{"
            "\"vibration\":%s,"
            "\"temperature\":%s,"
            "\"rpm\":%s"
        "},"
        "\"emergency_stop\":%s"
        "}",
        (unsigned int)g_system_state.sensors.timestamp,
        g_system_state.sensors.vibration,
        g_system_state.sensors.temperature,
        g_system_state.sensors.rpm,
        g_system_state.sensors.current,
        g_system_state.anomalies.health_score,
        g_system_state.anomalies.vibration_anomaly ? "true" : "false",
        g_system_state.anomalies.temperature_anomaly ? "true" : "false",
        g_system_state.anomalies.rpm_anomaly ? "true" : "false",
        g_system_state.emergency_stop ? "true" : "false"
    );
    
    return (uint32_t)len;
}

// Simulate network transmission
static bool transmit_packet(const char* packet, uint32_t size) {
    network_stats.transmission_in_progress = true;
    
    // Simulate transmission delay
    vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_TIME_MS));
    
    // Simulate occasional network failures (5% failure rate)
    bool success = (rand() % 100) >= 5;
    
    if (success) {
        network_stats.packets_sent++;
        network_stats.bytes_sent += size;
        
        // Track anomaly alerts
        if (g_system_state.anomalies.vibration_anomaly ||
            g_system_state.anomalies.temperature_anomaly ||
            g_system_state.anomalies.rpm_anomaly) {
            network_stats.anomaly_alerts_sent++;
        }
    } else {
        network_stats.packets_failed++;
        
        // Update connection state (protected) and clear event bit
        if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_system_state.mutex_stats.system_mutex_takes++;
            g_system_state.network_connected = false;
            g_system_state.event_group_stats.bits_cleared_count++;
            g_system_state.event_group_stats.current_event_bits &= ~NETWORK_CONNECTED_BIT;
            g_system_state.mutex_stats.system_mutex_gives++;
            xSemaphoreGive(xSystemStateMutex);
        } else {
            g_system_state.mutex_stats.system_mutex_timeouts++;
        }
        
        // Clear network connected bit in event group
        xEventGroupClearBits(xSystemReadyEvents, NETWORK_CONNECTED_BIT);
    }
    
    network_stats.transmission_in_progress = false;
    network_stats.last_transmission_time = xTaskGetTickCount();
    
    return success;
}

// Simulate network reconnection
static void check_network_reconnect(void) {
    bool was_connected = false;
    bool is_connected = false;
    
    // Get current connection state (protected)
    if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_system_state.mutex_stats.system_mutex_takes++;
        was_connected = g_system_state.network_connected;
        g_system_state.mutex_stats.system_mutex_gives++;
        xSemaphoreGive(xSystemStateMutex);
    } else {
        g_system_state.mutex_stats.system_mutex_timeouts++;
    }
    
    if (!was_connected) {
        // Try to reconnect (50% chance each attempt)
        if (rand() % 100 < 50) {
            is_connected = true;
            
            // Update connection state (protected)
            if (xSemaphoreTake(xSystemStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_system_state.mutex_stats.system_mutex_takes++;
                g_system_state.network_connected = true;
                g_system_state.event_group_stats.bits_set_count++;
                g_system_state.event_group_stats.current_event_bits |= NETWORK_CONNECTED_BIT;
                g_system_state.mutex_stats.system_mutex_gives++;
                xSemaphoreGive(xSystemStateMutex);
            } else {
                g_system_state.mutex_stats.system_mutex_timeouts++;
            }
            
            // Set network connected bit in event group
            xEventGroupSetBits(xSystemReadyEvents, NETWORK_CONNECTED_BIT);
        }
    }
}

void vNetworkTask(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(NETWORK_SEND_RATE_MS);
    
    uint32_t cycle_count = 0;
    
    while (1) {
        // Wait for the next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        cycle_count++;
        
        // Process anomaly alerts from queue (Capability 3)
        AnomalyAlert_t alert;
        int alerts_processed = 0;
        // Process only 1 alert per cycle (1Hz) to let queue build up
        if (xQueueReceive(xAnomalyAlertQueue, &alert, 0) == pdTRUE) {
            alerts_processed = 1;
            // In real system, would send alert immediately
            // Here we just count them
            network_stats.anomaly_alerts_sent++;
        }
        
        // Check network connection
        if (!g_system_state.network_connected) {
            check_network_reconnect();
            if (!g_system_state.network_connected) {
                continue;  // Skip transmission if not connected
            }
        }
        
        // Determine packet type based on system state and cycle (Capability 6)
        PacketType_t packet_type;
        if (cycle_count % 10 == 0) {
            // Every 10 seconds, send heartbeat packet (small)
            packet_type = PACKET_TYPE_HEARTBEAT;
        } else if (g_system_state.emergency_stop || g_system_state.anomalies.health_score < 50.0 || alerts_processed > 0) {
            // Emergency or anomaly conditions require large anomaly report packet
            packet_type = PACKET_TYPE_ANOMALY_REPORT;
        } else {
            // Normal operation uses medium sensor data packet
            packet_type = PACKET_TYPE_SENSOR_DATA;
        }
        
        // Allocate dynamic packet based on type
        PacketBuffer_t* packet = allocate_packet(packet_type);
        if (packet == NULL) {
            continue;  // Skip this cycle if allocation failed
        }
        
        // Create packet content based on type
        uint32_t content_size;
        if (packet_type == PACKET_TYPE_HEARTBEAT) {
            content_size = snprintf(packet->data, PACKET_HEARTBEAT_SIZE, "{\"heartbeat\":%u}", packet->timestamp);
        } else {
            content_size = create_packet(packet->data, 
                (packet_type == PACKET_TYPE_ANOMALY_REPORT) ? PACKET_ANOMALY_SIZE : PACKET_SENSOR_SIZE);
        }
        
        // Transmit packet and free memory
        bool success = transmit_packet(packet->data, content_size);
        
        // Free the dynamically allocated packet
        free_packet(packet);
        
        // Priority transmission for critical events  
        bool priority_transmission = false;
        if (g_system_state.emergency_stop ||
            g_system_state.anomalies.health_score < 50.0) {
            priority_transmission = true;
            
            // Record that we might preempt dashboard for critical transmission
            if (cycle_count % 3 == 0) {
                record_preemption("NetworkTask", "DashboardTask", "Critical");
            }
        }
        
        // This low-priority task often gets preempted
        // Demonstrate by recording when we resume after being preempted
        if (cycle_count % 5 == 0) {
            record_preemption("SensorTask", "NetworkTask", "Yield");
        }
        
        // Update context switch count
        // g_system_state.context_switch_count++; // Now tracked in update_task_stats()
        
        // Yield to other tasks
        if (!priority_transmission) {
            taskYIELD();
        }
    }
}
