# Example 07: Stack Overflow Detection

## Overview

This example demonstrates FreeRTOS stack overflow detection mechanisms, stack usage monitoring, and safe stack sizing strategies. It shows how to detect potential stack overflows before they cause system crashes and how to properly size task stacks for reliable operation.

## Key Features Demonstrated

### 1. Stack Overflow Detection Methods
- Method 1: Quick stack pointer check
- Method 2: Stack pattern check (0xA5A5A5A5)
- Overflow hook implementation
- Safe failure handling

### 2. Stack Usage Monitoring
- High water mark tracking
- Real-time usage visualization
- Per-task monitoring
- Peak usage tracking

### 3. Stack Usage Patterns
- Minimal usage baseline
- Moderate with local arrays
- Heavy with printf/formatting
- Recursive function calls
- Large array allocations

### 4. Stack Sizing Strategies
- Conservative initial sizing
- Runtime profiling
- Usage-based optimization
- Safety margins

## System Architecture

```
Stack Layout (grows downward):
┌─────────────────┐ ← Stack Top (high address)
│   Parameters    │
│ Return Address  │
│ Saved Registers │
├─────────────────┤ ← Stack Pointer
│ Local Variables │ (Current usage)
│       ↓         │
│   Free Stack    │ (Painted with 0xA5)
│       ↓         │
├─────────────────┤ ← High Water Mark
│  Used Stack     │ (Deepest usage)
│   0xA5A5A5A5    │
│   0xA5A5A5A5    │ (Unused - pattern intact)
└─────────────────┘ ← Stack Bottom (low address)

Task Stack Monitoring:
┌──────────────────────────────────┐
│ Task Name │ Size │ Used │ Free   │
├──────────────────────────────────┤
│ Minimal   │ 512  │ 128  │ 384    │ [###-----------------] 25%
│ Moderate  │ 1024 │ 512  │ 512    │ [##########----------] 50%
│ Heavy     │ 2048 │ 1536 │ 512    │ [###############-----] 75%
│ Recursion │ 2048 │ 1800 │ 248    │ [##################--] 88% WARNING!
└──────────────────────────────────┘
```

## Tasks Description

### 1. Minimal Task (512 bytes stack)
- Baseline for minimum stack usage
- Simple counter increment
- Occasional printf
- Shows minimum viable stack size

### 2. Moderate Task (1024 bytes stack)
- Local arrays (128 + 32*4 bytes)
- String formatting
- Typical embedded task usage
- Good for most simple tasks

### 3. Heavy Task (2048 bytes stack)
- Large local arrays (64 floats + 256 char)
- Heavy printf with floating point
- Statistical calculations
- Needs generous stack allocation

### 4. Recursion Task (2048 bytes stack)
- Controlled recursive function calls
- Each level uses ~100 bytes
- Demonstrates stack growth
- Tests depth limits safely

### 5. Array Task (1024 bytes stack)
- Variable-length array allocation
- Tests available stack space
- Demonstrates dangerous patterns
- Shows VLA risks

### 6. Monitor Task (1024 bytes stack)
- Tracks all task stacks
- Updates statistics
- Generates reports
- Issues warnings

### 7. Control Task (1024 bytes stack)
- Enables test scenarios
- Rotates through tests
- Prevents continuous stress
- Safe test orchestration

## Expected Output

```
===========================================
Example 07: Stack Overflow Detection
Monitoring and protection demonstration
===========================================

Configuration:
  Stack overflow check: Method 2 (Pattern check)
  Stack sizes:
    MINIMAL:  512 bytes
    NORMAL:   1024 bytes
    LARGE:    2048 bytes
    HUGE:     4096 bytes

[DEMO] Stack Painting Visualization:
  Unused stack filled with: 0xA5
  High water mark = deepest stack usage
  Pattern intact = stack never used
  Pattern gone = stack was used

[MINIMAL] Task started with 512 bytes stack
[MODERATE] Task started with 1024 bytes stack
[HEAVY] Task started with 2048 bytes stack
[RECURSION] Task started with 2048 bytes stack

[CONTROL] Starting test cycle 1
[CONTROL] Enabling recursion test (safe depth)
[RECURSION] Starting recursion test...
[RECURSION] Depth: 10, Stack: 450 words free
[RECURSION] Completed depth 10, result: 56

========================================
STACK USAGE REPORT
========================================
Task            Size     Used     Free  Min Free  Usage
----            ----     ----     ----  --------  -----
Minimal          512      128      384       380  [#####---------------]  25%
Moderate        1024      456      568       512  [#########-----------]  44%
Heavy           2048     1024     1024       896  [##########----------]  50%
Recursion       2048     1280      768       640  [#############-------]  62%
Array           1024      384      640       600  [########------------]  37%
Monitor         1024      512      512       480  [##########----------]  50%
Control         1024      320      704       680  [######--------------]  31%
========================================
Note: Sizes in bytes. Lower 'Min Free' = higher usage
========================================

[WARNING] Task 'Recursion' stack usage > 80% (85%)

[CONTROL] Starting test cycle 2
[CONTROL] Enabling array allocation test
[ARRAY] Testing large array allocation...
[ARRAY] Stack before: 150 words free
[ARRAY] Stack after: 75 words free
[ARRAY] Used 300 bytes for array
```

## Stack Overflow Scenarios

### 1. Safe Recursion Test
```c
// Controlled depth with monitoring
recursive_function(10);  // Safe depth
// Each level: ~100 bytes
// Total: 1000 bytes (within 2048 limit)
```

### 2. Array Allocation Test
```c
// Check available stack first
size_t safe_size = (free_stack * sizeof(StackType_t)) / 2;
volatile uint8_t array[safe_size];  // Use half of free
```

### 3. Printf Heavy Test
```c
// Printf with floats uses significant stack
printf("Sensor: %.2f, %.2f, %.2f\n", f1, f2, f3);
// Can use 200+ bytes for formatting
```

## Detection Methods Comparison

| Method | Detection | Speed | Reliability | Overhead |
|--------|-----------|-------|-------------|----------|
| Method 0 | None | N/A | None | None |
| Method 1 | Stack pointer check | Fast | Good | Low |
| Method 2 | Pattern check | Slower | Excellent | Medium |

### Method 1: Quick Check
- Checks if stack pointer exceeds bounds
- Done at context switch
- Might miss temporary overflows
- Good for production

### Method 2: Pattern Check
- Fills stack with 0xA5A5A5A5
- Checks if pattern corrupted
- Catches all overflows
- Better for development

## Stack Sizing Guidelines

### 1. Initial Sizing Formula
```
Base:        200 bytes (FreeRTOS overhead)
+ Locals:    Sum of all local variables
+ Call depth: Max depth × frame size
+ Formatting: 200 bytes if using printf
+ Margin:    25-50% safety factor
= Total Stack Size
```

### 2. Common Stack Sizes
| Task Type | Typical Size | Use Case |
|-----------|-------------|----------|
| Minimal | 512 bytes | Simple state machines |
| Standard | 1024 bytes | Most tasks |
| Complex | 2048 bytes | Printf, calculations |
| Heavy | 4096 bytes | Network, file systems |

### 3. Optimization Process
1. Start with generous size (2-4KB)
2. Run all code paths
3. Check high water mark
4. Reduce to: `(peak usage × 1.5)`
5. Round up to power of 2

## Exercises

1. **Trigger Overflow**
   - Reduce NORMAL_STACK_SIZE to 512
   - Watch overflow hook trigger
   - System halts safely

2. **Optimize Stack Sizes**
   - Run for extended period
   - Note minimum free values
   - Calculate optimal sizes

3. **Test Deep Recursion**
   - Increase recursion depth gradually
   - Find maximum safe depth
   - Calculate bytes per level

4. **Profile Printf Usage**
   - Measure stack before/after printf
   - Try different format strings
   - Find worst-case usage

5. **Implement Recovery**
   - Modify overflow hook
   - Log to non-volatile memory
   - Attempt graceful restart

## Wind Turbine Application

Stack requirements for wind turbine tasks:

| Task | Stack Size | Justification |
|------|------------|---------------|
| Safety | 2048 bytes | Critical, needs margin |
| Sensor | 1024 bytes | Moderate processing |
| Network | 2048 bytes | Printf, buffers |
| Control | 1536 bytes | State machine, calculations |
| Logger | 1024 bytes | Simple file writes |
| Monitor | 768 bytes | Minimal requirements |

## Building and Running

```bash
# Build
cd ../..
./scripts/build.sh simulation

# Run
./build/simulation/examples/07_stack_check/stack_check_example

# Observe:
# - Stack usage reports every 5 seconds
# - Test cycles every 10 seconds
# - Warnings for high usage
# - Pattern check results
```

## Troubleshooting

### Stack Overflow Detected
- Increase stack size
- Reduce local variables
- Minimize recursion depth
- Move large data to heap

### High Stack Usage
- Profile actual usage
- Identify heavy functions
- Optimize local variables
- Consider static allocation

### Method Not Working
- Verify configCHECK_FOR_STACK_OVERFLOW
- Check hook implementation
- Ensure not optimized out
- Test with known overflow

## Key Takeaways

1. **Enable detection during development** - Use Method 2
2. **Monitor continuously** - Track high water marks
3. **Size conservatively** - Add safety margins
4. **Profile thoroughly** - Test all code paths
5. **Handle overflows** - Implement safe recovery
6. **Document requirements** - Record stack sizes
7. **Avoid large locals** - Use heap for big data
8. **Minimize recursion** - Use iterative when possible
9. **Watch printf usage** - Significant stack consumer
10. **Test edge cases** - Verify worst-case scenarios

## Integration with Final System

This stack overflow detection capability ensures reliability in the integrated Wind Turbine Monitor:

### Direct Applications

1. **Safety-Critical Tasks**
   - Vibration monitoring task: 2048 bytes
   - Emergency response task: 1536 bytes
   - Guaranteed stack safety margins

2. **Stack Size Optimization**
   - Profile actual usage patterns
   - Right-size each task's stack
   - Save RAM for other uses

3. **Runtime Protection**
   - Detect overflows before crashes
   - Log stack usage trends
   - Preventive maintenance alerts

### Console Dashboard Preview

```
STACK MONITOR                         Status: HEALTHY
----------------------------------------------------------
TASK STACKS:
  VibrationTask:  [########------------] 40% (819/2048)
  NetworkTask:    [############--------] 60% (1229/2048)
  SensorTask:     [######--------------] 30% (307/1024)
  SafetyTask:     [####----------------] 20% (307/1536)
  
WARNINGS:
  NetworkTask approaching limit (>80% usage expected)
  
STATISTICS:
  Overflow Checks: 15234    Violations: 0
  Pattern Intact: YES        Max Usage: 85% (NetworkTask)
```

## Next Steps

After mastering stack overflow detection:
- Example 08: Power Management with Tickless Idle
- Then integrate all 8 capabilities into complete system
- Deploy threshold-based anomaly detection
- Create production-ready wind turbine monitor