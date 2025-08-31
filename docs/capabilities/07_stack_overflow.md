# Capability 7: Stack Overflow Detection

## Overview

Stack overflow occurs when a task uses more stack space than allocated, corrupting adjacent memory regions. This is one of the most common causes of mysterious crashes in embedded systems. FreeRTOS provides built-in mechanisms to detect stack overflows before they cause system failure.

## Understanding Stack Overflow

### What Causes Stack Overflow?

```c
// 1. Large local variables
void bad_function(void) {
    uint8_t huge_array[10000];  // 10KB on stack!
    // Stack overflow likely
}

// 2. Deep recursion
void recursive_function(int depth) {
    char buffer[100];  // 100 bytes per call
    if (depth > 0) {
        recursive_function(depth - 1);  // Stack grows
    }
}

// 3. Deep call chains
void func_a() { func_b(); }
void func_b() { func_c(); }
void func_c() { func_d(); }
// ... many levels deep

// 4. Variable length arrays (VLA)
void dangerous_function(size_t n) {
    char buffer[n];  // Runtime size - dangerous!
}
```

### Stack Layout and Growth

```
High Memory
┌─────────────────┐
│  Other Memory   │
├─────────────────┤ ← Stack Base (high address)
│  Stack Guard    │ (Optional guard region)
├─────────────────┤
│  Parameters     │
│  Return Address │
│  Saved Registers│
│  Local Variables│ ← Stack Pointer (grows down)
│       ↓         │
│   Free Stack    │
│       ↓         │
├─────────────────┤ ← Stack Limit (low address)
│  Task TCB       │
└─────────────────┘
Low Memory

Stack grows DOWN (from high to low addresses)
Overflow occurs when SP goes below Stack Limit
```

## FreeRTOS Detection Methods

### Method 1: Fast Stack Pointer Check

```c
// In FreeRTOSConfig.h
#define configCHECK_FOR_STACK_OVERFLOW 1

// How it works:
// - Checks if stack pointer exceeded bounds
// - Performed during context switch
// - Fast but might miss transient overflows
// - Good for production systems

// Detection points:
// 1. Context switch OUT (task being switched from)
// 2. Checks: CurrentStackPointer < StackLimitAddress

// Pros: Low overhead, catches most overflows
// Cons: Might miss temporary overflows that recover
```

### Method 2: Stack Pattern Check

```c
// In FreeRTOSConfig.h
#define configCHECK_FOR_STACK_OVERFLOW 2

// How it works:
// - Fills stack with pattern (0xA5A5A5A5)
// - Checks if pattern corrupted
// - More thorough than Method 1
// - Better for development/debugging

// Implementation:
// 1. Stack initialized with pattern at task creation
// 2. Last 16 bytes checked for pattern
// 3. If pattern gone = overflow occurred

// Pros: Catches all overflows, even temporary
// Cons: Higher overhead, pattern check takes time
```

### Overflow Hook Function

```c
// Called when overflow detected
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // Critical: System is compromised!
    // Stack has already overflowed
    // Memory corruption may have occurred
    
    // Log the error (if safe to do so)
    printf("STACK OVERFLOW: Task %s\n", pcTaskName);
    
    // Common responses:
    // 1. Halt system (safest)
    taskDISABLE_INTERRUPTS();
    for(;;);
    
    // 2. Reset system
    // NVIC_SystemReset();
    
    // 3. Try to recover (risky)
    // vTaskDelete(xTask);
}
```

## Stack Monitoring APIs

### High Water Mark

```c
// Get minimum free stack space ever for a task
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask) {
    // Returns minimum free stack in words
    // NULL = current task
    
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    
    // Convert to bytes (platform dependent)
    size_t bytes_free = uxHighWaterMark * sizeof(StackType_t);
    
    if (bytes_free < 100) {
        printf("WARNING: Stack nearly full!\n");
    }
}

// Monitor all tasks
void monitor_all_stacks(void) {
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    
    if (pxTaskStatusArray != NULL) {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, 
                                          uxArraySize, NULL);
        
        for (UBaseType_t i = 0; i < uxArraySize; i++) {
            printf("Task: %s, Stack Free: %u\n",
                   pxTaskStatusArray[i].pcTaskName,
                   pxTaskStatusArray[i].usStackHighWaterMark);
        }
        
        vPortFree(pxTaskStatusArray);
    }
}
```

### Stack Usage Calculation

```c
// Calculate stack usage percentage
uint8_t get_stack_usage_percent(TaskHandle_t xTask, size_t stack_size) {
    UBaseType_t high_water = uxTaskGetStackHighWaterMark(xTask);
    UBaseType_t used = (stack_size / sizeof(StackType_t)) - high_water;
    
    return (uint8_t)((used * 100) / (stack_size / sizeof(StackType_t)));
}

// Visual stack usage
void print_stack_usage(TaskHandle_t xTask, size_t stack_size) {
    uint8_t percent = get_stack_usage_percent(xTask, stack_size);
    
    printf("Stack [");
    for (int i = 0; i < 20; i++) {
        if (i < (percent / 5)) {
            printf("#");
        } else {
            printf("-");
        }
    }
    printf("] %u%%\n", percent);
}
```

## Stack Sizing Strategies

### 1. Empirical Method

```c
// Start with generous size
#define INITIAL_STACK_SIZE (1024 * 4)  // 4KB

// Run all code paths
// Monitor high water mark
// Reduce to: HighWaterMark + 25% margin

// Example:
// High water mark: 800 bytes used
// Add 25% margin: 800 * 1.25 = 1000 bytes
// Round up: 1024 bytes final size
```

### 2. Static Analysis

```c
// Calculate stack requirements:
// 1. Count local variables
// 2. Add function call overhead (registers, return address)
// 3. Multiply by maximum call depth
// 4. Add interrupt overhead if applicable

// Example calculation:
void task_function() {           // Stack usage:
    char buffer[256];            // 256 bytes
    int array[64];               // 256 bytes
    struct_t data;               // 128 bytes
    call_function_a();           // + their stack
    call_function_b();           // + their stack
}                                // Total: 640 + calls

// Add 50% safety margin for production
```

### 3. Runtime Profiling

```c
// Profile actual usage patterns
typedef struct {
    TaskHandle_t task;
    size_t configured_size;
    UBaseType_t high_water_mark;
    uint8_t usage_percent;
    bool warning_issued;
} StackProfile_t;

static StackProfile_t profiles[MAX_TASKS];

void profile_stack_usage(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (profiles[i].task != NULL) {
            UBaseType_t current = uxTaskGetStackHighWaterMark(profiles[i].task);
            
            if (current < profiles[i].high_water_mark) {
                profiles[i].high_water_mark = current;
                profiles[i].usage_percent = calculate_percent(...);
                
                if (profiles[i].usage_percent > 80 && !profiles[i].warning_issued) {
                    printf("WARNING: Task %s using %u%% of stack\n",
                           pcTaskGetName(profiles[i].task),
                           profiles[i].usage_percent);
                    profiles[i].warning_issued = true;
                }
            }
        }
    }
}
```

## Stack Painting Technique

### How Stack Painting Works

```c
// FreeRTOS fills unused stack with pattern
#define tskSTACK_FILL_BYTE (0xA5U)

// At task creation:
// 1. Entire stack filled with 0xA5A5A5A5
// 2. As task runs, pattern overwritten
// 3. High water mark = where pattern stops

// Visual representation:
// ┌─────────────┐
// │ Used Stack  │ (Various values)
// ├─────────────┤ ← High Water Mark
// │ 0xA5A5A5A5  │ (Unused - pattern intact)
// │ 0xA5A5A5A5  │
// │ 0xA5A5A5A5  │
// └─────────────┘

// Manual stack painting check
bool check_stack_pattern(uint8_t *stack_bottom, size_t size) {
    uint8_t *p = stack_bottom;
    size_t unused = 0;
    
    while (p < (stack_bottom + size)) {
        if (*p == tskSTACK_FILL_BYTE) {
            unused++;
        } else {
            break;  // Found used portion
        }
        p++;
    }
    
    printf("Stack: %zu bytes unused of %zu total\n", unused, size);
    return (unused > 100);  // Return true if safe margin
}
```

## Recovery Strategies

### 1. Graceful Degradation

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // Log error to non-volatile memory
    log_critical_error(STACK_OVERFLOW, pcTaskName);
    
    // Enter safe mode
    enter_safe_mode();
    
    // Disable non-critical features
    disable_feature(FEATURE_LOGGING);
    disable_feature(FEATURE_TELEMETRY);
    
    // Continue with reduced functionality
}
```

### 2. Task Restart

```c
// Risky but sometimes necessary
void restart_overflowed_task(TaskHandle_t xTask, char *pcTaskName) {
    // Save task parameters
    TaskParameters_t params = get_task_params(xTask);
    
    // Delete corrupted task
    vTaskDelete(xTask);
    
    // Increase stack size
    params.stack_size *= 2;
    
    // Recreate task
    xTaskCreate(params.function,
                pcTaskName,
                params.stack_size,
                params.parameters,
                params.priority,
                &xTask);
    
    // Log incident
    log_stack_overflow_recovery(pcTaskName, params.stack_size);
}
```

### 3. System Reset with Diagnostics

```c
void safe_system_reset(TaskHandle_t xTask, char *pcTaskName) {
    // Save diagnostic info to persistent storage
    DiagnosticInfo_t info = {
        .error_type = STACK_OVERFLOW,
        .task_name = pcTaskName,
        .timestamp = xTaskGetTickCount(),
        .stack_remaining = uxTaskGetStackHighWaterMark(xTask)
    };
    
    save_to_eeprom(&info);
    
    // Trigger watchdog reset
    while(1);  // Let watchdog reset system
}
```

## Best Practices

### 1. Conservative Initial Sizing

```c
// Development: Large stacks
#ifdef DEBUG
    #define TASK_STACK_SIZE (4096)
#else
    #define TASK_STACK_SIZE (1024)
#endif

// Profile first, optimize later
```

### 2. Regular Monitoring

```c
// Periodic stack check task
void vStackMonitorTask(void *pvParameters) {
    for (;;) {
        monitor_all_stacks();
        vTaskDelay(pdMS_TO_TICKS(10000));  // Every 10 seconds
    }
}
```

### 3. Guard Regions

```c
// Add guard zones between stacks
#define STACK_GUARD_SIZE 32

// Allocate with guards
uint8_t stack[TASK_STACK_SIZE + STACK_GUARD_SIZE];

// Fill guards with pattern
memset(stack, 0xDE, STACK_GUARD_SIZE);  // Dead beef pattern
memset(stack + TASK_STACK_SIZE, 0xDE, STACK_GUARD_SIZE);

// Check guards periodically
if (stack[0] != 0xDE || stack[TASK_STACK_SIZE] != 0xDE) {
    // Overflow detected!
}
```

## Common Stack Overflow Scenarios

### 1. Printf and Formatting

```c
// printf uses significant stack!
void bad_logging(float values[], int count) {
    char buffer[1024];  // Large buffer
    
    for (int i = 0; i < count; i++) {
        sprintf(buffer, "Value[%d] = %.6f\n", i, values[i]);
        // sprintf can use lots of stack for float formatting
    }
}

// Better: Use smaller buffer or heap
void good_logging(float values[], int count) {
    for (int i = 0; i < count; i++) {
        printf("Value[%d] = %.6f\n", i, values[i]);
        // Let printf handle buffering
    }
}
```

### 2. Recursive Algorithms

```c
// Dangerous recursive search
int recursive_search(Node *node, int value) {
    if (node == NULL) return -1;
    if (node->value == value) return node->index;
    
    // Recursion can overflow with deep trees
    return recursive_search(node->next, value);
}

// Better: Iterative version
int iterative_search(Node *node, int value) {
    while (node != NULL) {
        if (node->value == value) return node->index;
        node = node->next;
    }
    return -1;
}
```

### 3. Large Structures

```c
// Bad: Large structure on stack
void process_data(void) {
    SensorData_t data[100];  // Could be KB of stack!
    // Process...
}

// Good: Use heap or static
void process_data(void) {
    static SensorData_t data[100];  // In .bss, not stack
    // Or
    SensorData_t *data = pvPortMalloc(sizeof(SensorData_t) * 100);
    // Process...
    vPortFree(data);
}
```

## Wind Turbine Application

### Safety-Critical Stack Protection

```c
// Wind turbine task stack requirements
#define SAFETY_TASK_STACK      (2048)  // Critical - generous size
#define SENSOR_TASK_STACK      (1024)  // Moderate requirements
#define NETWORK_TASK_STACK     (1536)  // Printf/formatting
#define LOGGING_TASK_STACK     (768)   // Simple operations

// Create tasks with appropriate stacks
xTaskCreate(vSafetyTask, "Safety", SAFETY_TASK_STACK, ...);
xTaskCreate(vSensorTask, "Sensor", SENSOR_TASK_STACK, ...);

// Monitor critical tasks more frequently
void monitor_critical_stacks(void) {
    UBaseType_t safety_free = uxTaskGetStackHighWaterMark(xSafetyTask);
    
    if (safety_free < 200) {
        // Critical warning!
        trigger_maintenance_mode();
        increase_stack_size(xSafetyTask);
    }
}
```

## Testing Stack Overflow Detection

### 1. Controlled Overflow Test

```c
void test_stack_overflow(void) {
    // WARNING: Will trigger overflow!
    volatile uint8_t large_array[10000];
    
    // Fill array to ensure stack usage
    for (int i = 0; i < 10000; i++) {
        large_array[i] = i;
    }
}
```

### 2. Recursion Test

```c
void test_recursion(int depth) {
    char buffer[100];
    sprintf(buffer, "Depth: %d", depth);
    
    if (depth > 0) {
        test_recursion(depth - 1);
    }
}
```

### 3. High Water Mark Test

```c
void test_high_water_mark(void) {
    UBaseType_t initial = uxTaskGetStackHighWaterMark(NULL);
    
    // Use some stack
    char buffer[500];
    memset(buffer, 0, sizeof(buffer));
    
    UBaseType_t after = uxTaskGetStackHighWaterMark(NULL);
    
    printf("Stack used: %u words\n", initial - after);
}
```

## Summary

Stack overflow detection is critical for reliable embedded systems:

1. **Enable detection** - Use Method 2 during development
2. **Size conservatively** - Profile then optimize
3. **Monitor continuously** - Track high water marks
4. **Handle overflows** - Implement recovery strategy
5. **Test thoroughly** - Verify all code paths
6. **Document requirements** - Record stack sizes needed
7. **Use static analysis** - Calculate worst-case usage
8. **Avoid large locals** - Use heap for big data
9. **Minimize recursion** - Prefer iterative algorithms
10. **Guard critical tasks** - Extra margin for safety

## Integration with Wind Turbine Monitor

Capability 7 is fully integrated into the main Wind Turbine Monitor system with proactive monitoring:

### Proactive Stack Monitoring
Instead of waiting for crashes, the integrated system continuously monitors stack health:

```c
// dashboard_task.c - Proactive health checking
static void check_stack_health(void) {
    UBaseType_t my_stack_free = uxTaskGetStackHighWaterMark(NULL);
    if (my_stack_free < 100) {
        printf("[STACK HEALTH] WARNING: Dashboard task low stack!\n");
    }
}
```

### Real-Time Dashboard Display
Stack status is prominently displayed in the live dashboard:

```
STACK STATUS:
  Monitored Tasks: 7 | Warnings: 0 | Critical: 0 | Overflows: 0
  Task Stack Usage:
    DashboardTask  18% (Peak: 18%, Free: 1019 words)
    NetworkTask   10% (Peak: 10%, Free: 507 words) 
    SensorTask     8% (Peak: 8%, Free: 507 words)
    AnomalyTask   15% (Peak: 15%, Free: 507 words)
  Proactive Checks: 854 (Good coding practice!)
```

### Key Integration Features
- **Real FreeRTOS measurements** using `uxTaskGetStackHighWaterMark()` API
- **Color-coded warnings** (green <70%, yellow 70-85%, red >85%)
- **Comprehensive statistics** tracking for all monitored tasks
- **Self-monitoring** where tasks check their own stack health
- **Good coding practices** demonstrated with proactive vs reactive approach

See the [Integrated System README](../../src/integrated/README.md) for complete implementation details.

Stack overflow detection prevents mysterious crashes and ensures system reliability!