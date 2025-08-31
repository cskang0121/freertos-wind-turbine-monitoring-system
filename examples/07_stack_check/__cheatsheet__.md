# Stack Overflow Detection and Prevention - Quick Reference

## Overview
This capability demonstrates stack overflow detection and prevention using FreeRTOS built-in mechanisms. Stack overflow is one of the most dangerous embedded system faults, potentially causing data corruption and system crashes.

**Key Concepts:**
- Stack memory layout and growth patterns
- Two detection methods: pointer checking and pattern checking
- Stack painting technique for usage monitoring
- High water mark calculation for optimization
- Prevention through proper sizing and monitoring

## Memory Architecture Understanding

### Memory Region Comparison
| Region | Allocation | Lifetime | Size | Speed | Overflow Risk |
|--------|------------|----------|------|-------|---------------|
| **Static** | Compile-time | Forever | Fixed | Fastest | None |
| **Heap** | Runtime | Manual free | Variable | Medium | Low |
| **Stack** | Function calls | Automatic | Per-task | Fast | **HIGH** |

### Stack Memory Layout
```
High Address  ┌──────────────┐ ← pxTopOfStack
              │   Unused     │
         SP → ├──────────────┤ ← Current Stack Pointer
              │  Variables   │ Stack grows DOWN ↓
              │ Return Addr  │
      HWM →   ├──────────────┤ ← High Water Mark
              │              │
              ├──────────────┤ ← Usable Limit
              │ A5A5A5A5     │ ← Guard Zone (FreeRTOS)
Low Address   └──────────────┘ ← pxStack (bottom)
```

## Core FreeRTOS Stack Detection

### 1. Configuration Options
```c
// FreeRTOSConfig.h - Enable stack overflow detection
#define configCHECK_FOR_STACK_OVERFLOW    2    // Method 2 (recommended)

// Method 1: Fast pointer checking only
// Method 2: Pattern checking (thorough)
```

### 2. Detection Methods
```c
// Method 1: Stack Pointer Checking
#if (configCHECK_FOR_STACK_OVERFLOW == 1)
    if (pxCurrentTCB->pxTopOfStack <= pxCurrentTCB->pxStack) {
        vApplicationStackOverflowHook(pxCurrentTCB, pxCurrentTCB->pcTaskName);
    }
#endif

// Method 2: Guard Pattern Checking  
#if (configCHECK_FOR_STACK_OVERFLOW == 2)
    // Check last 16 bytes for 0xA5A5A5A5 pattern
    if (pattern_corrupted) {
        vApplicationStackOverflowHook(pxCurrentTCB, pxCurrentTCB->pcTaskName);
    }
#endif
```

### 3. Stack Monitoring APIs
```c
// Get remaining stack space
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);

// Get current task's remaining stack
UBaseType_t remaining = uxTaskGetStackHighWaterMark(NULL);
```

## Implementation Patterns

### 1. Stack Overflow Hook (Required)
```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // CRITICAL: System is corrupted - be extremely careful
    
    // Disable interrupts immediately
    taskDISABLE_INTERRUPTS();
    
    // Try to log error (may fail if corruption severe)
    printf("FATAL: Stack overflow in task %s\n", pcTaskName);
    
    // Save crash info to non-volatile storage
    save_crash_info(pcTaskName, xTaskGetTickCount());
    
    // Halt or reset system
    #ifdef DEBUG
        __BKPT(0);  // Break for debugger
        for(;;);
    #else
        system_reset();  // Trigger watchdog reset
    #endif
}
```

### 2. Stack Usage Monitoring
```c
typedef struct {
    TaskHandle_t handle;
    const char *name;
    size_t stackSize;
    UBaseType_t highWaterMark;
    uint8_t maxUsagePercent;
    bool warningIssued;
} StackMonitor_t;

void vStackMonitorTask(void *pvParameters) {
    const TickType_t xCheckPeriod = pdMS_TO_TICKS(5000);
    
    while(1) {
        // Check all registered tasks
        for(int i = 0; i < MAX_MONITORED_TASKS; i++) {
            if(stackMonitors[i].handle != NULL) {
                updateStackUsage(&stackMonitors[i]);
            }
        }
        
        vTaskDelay(xCheckPeriod);
    }
}

void updateStackUsage(StackMonitor_t *monitor) {
    UBaseType_t current = uxTaskGetStackHighWaterMark(monitor->handle);
    
    // Calculate usage percentage
    size_t stackWords = monitor->stackSize / sizeof(StackType_t);
    size_t usedWords = stackWords - current;
    uint8_t usagePercent = (usedWords * 100) / stackWords;
    
    // Track maximum usage
    if(usagePercent > monitor->maxUsagePercent) {
        monitor->maxUsagePercent = usagePercent;
    }
    
    // Issue warnings
    if(usagePercent > 80 && !monitor->warningIssued) {
        printf("WARNING: Task %s using %u%% of stack\n", 
               monitor->name, usagePercent);
        monitor->warningIssued = true;
    }
    
    if(usagePercent > 95) {
        printf("CRITICAL: Task %s at %u%% stack usage!\n",
               monitor->name, usagePercent);
    }
}
```

### 3. Stack Size Calculation
```c
typedef struct {
    size_t localVariables;      // Sum of all local vars
    size_t maxCallDepth;        // Maximum call chain
    size_t functionOverhead;    // Per-function overhead (~32 bytes)
    bool usesPrintf;            // Printf needs 300-500 bytes
    bool usesFloatingPoint;     // FP operations need 200+ bytes
    size_t interruptContext;    // ISR stack usage (~100 bytes)
    float safetyMargin;         // 1.25 to 2.0 multiplier
} StackRequirements_t;

size_t calculateStackSize(const StackRequirements_t *req) {
    size_t total = 200;  // Base FreeRTOS overhead
    
    total += req->localVariables;
    total += req->maxCallDepth * req->functionOverhead;
    total += req->interruptContext;
    
    if(req->usesPrintf) {
        total += 400;  // Printf internal buffers
    }
    
    if(req->usesFloatingPoint) {
        total += 200;  // FP library routines
    }
    
    // Apply safety margin
    total = (size_t)(total * req->safetyMargin);
    
    // Round up to nearest power of 2
    size_t powerOf2 = 512;
    while(powerOf2 < total) {
        powerOf2 *= 2;
    }
    
    return powerOf2;
}
```

## Common Stack Overflow Causes

### 1. Large Local Variables
```c
// BAD: Large arrays on stack
void dangerous_function(void) {
    char hugeBuffer[2048];  // 2KB on stack!
    int largeArray[500];    // 2KB more!
    // Total: 4KB just for locals
}

// GOOD: Use heap for large data
void safe_function(void) {
    char *buffer = pvPortMalloc(2048);
    if(buffer != NULL) {
        // Use buffer
        vPortFree(buffer);
    }
}
```

### 2. Printf with Floating Point
```c
// BAD: Printf uses lots of stack
void bad_printf(void) {
    float values[10];
    // Printf with floats can use 400-800 bytes!
    printf("Values: %.6f, %.6f, %.6f\n", values[0], values[1], values[2]);
}

// GOOD: Simple format or separate calls
void good_printf(void) {
    float value;
    for(int i = 0; i < 10; i++) {
        value = getValues(i);
        printf("Value %d: %.2f\n", i, value);  // Much less stack
    }
}
```

### 3. Unbounded Recursion
```c
// BAD: Recursive without depth limit
int factorial(int n) {
    if(n <= 1) return 1;
    return n * factorial(n - 1);  // Unlimited recursion!
}

// GOOD: Iterative or depth-limited
int factorial_safe(int n) {
    int result = 1;
    for(int i = 2; i <= n; i++) {
        result *= i;  // Constant stack usage
    }
    return result;
}
```

## Performance Considerations

### Detection Method Comparison
- **Method 1**: ~2 CPU cycles per context switch
- **Method 2**: ~20 CPU cycles per context switch
- **Recommendation**: Use Method 2 for production systems

### Monitoring Overhead
- High water mark check: ~10 cycles per call
- Pattern verification: ~50 cycles per full check
- **Strategy**: Check every 5-10 seconds, not every task switch

## Integration with Wind Turbine System

### Stack Size Allocation
Our system uses carefully calculated stack sizes:

```c
// Task stack definitions based on profiling
#define SENSOR_TASK_STACK       (1024)    // 1KB - basic sensors
#define ANOMALY_TASK_STACK      (2048)    // 2KB - complex algorithms  
#define NETWORK_TASK_STACK      (1536)    // 1.5KB - TCP/IP stack
#define SAFETY_TASK_STACK       (512)     // 512B - critical, minimal
#define DASHBOARD_TASK_STACK    (1024)    // 1KB - display formatting
```

### Runtime Stack Monitoring
The system continuously monitors stack health:
- **Check interval**: Every 5 seconds
- **Warning threshold**: 80% usage
- **Critical threshold**: 95% usage
- **Action on critical**: Log event and increase monitoring frequency

### Stack Overflow Recovery
When overflow occurs:
1. **Log critical error** with task name and timestamp
2. **Save system state** to EEPROM for post-mortem analysis
3. **Trigger controlled reset** to restore system operation
4. **Increment boot counter** to detect repeated failures

## Best Practices

- **Enable Method 2 detection**: Provides comprehensive overflow catching
- **Size stacks generously**: Add 25-50% safety margin during development
- **Monitor continuously**: Track high water marks in production
- **Profile thoroughly**: Test all code paths and worst-case scenarios
- **Document requirements**: Record stack calculations for each task
- **Use heap for large data**: Keep stack usage predictable
- **Avoid deep call chains**: Limit function nesting depth
- **Test overflow handling**: Verify recovery mechanisms work correctly