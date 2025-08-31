# Power Management with Tickless Idle - Quick Reference

## Overview
This capability demonstrates power-efficient operation using FreeRTOS tickless idle mode. Tickless idle suppresses unnecessary tick interrupts when the system is idle, dramatically reducing power consumption in battery-powered embedded systems.

**Key Concepts:**
- Tickless idle operation and tick suppression
- Dynamic sleep duration calculation
- Pre/post sleep processing hooks
- Wake source management and configuration
- Power state transitions and frequency scaling
- Battery-aware power profile adaptation

## Tickless Idle Fundamentals

### Traditional vs Tickless Operation
| Mode | Wake Frequency | Power Impact | CPU Utilization |
|------|---------------|--------------|-----------------|
| **Traditional Tick** | 1000 Hz (every 1ms) | High - constant wakeup | 10-20% idle overhead |
| **Tickless Idle** | Event-driven only | Low - sleep until needed | <1% idle overhead |

### Core Mechanism
```
Traditional:  T-T-T-T-T-T-T-T-T-T-T-T (1000 wakes/sec)
Tickless:     T----SLEEP(47ms)----T---SLEEP(123ms)---T (10 wakes/sec)
```

## Core FreeRTOS Power Management

### 1. Configuration Enable
```c
// FreeRTOSConfig.h - Enable tickless idle
#define configUSE_TICKLESS_IDLE                  1

// Minimum idle time before sleeping (ticks)
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP    2

// Enable pre/post sleep processing
#define configPRE_SLEEP_PROCESSING(x)            vApplicationPreSleepProcessing(&x)
#define configPOST_SLEEP_PROCESSING(x)           vApplicationPostSleepProcessing(x)

// Platform-specific sleep function
#define portSUPPRESS_TICKS_AND_SLEEP(x)         vPortSuppressTicksAndSleep(x)
```

### 2. Sleep Duration Calculation
```c
// FreeRTOS automatically calculates sleep time by:
// 1. Check ready list for next task wake time
// 2. Check timer list for next timer expiry
// 3. Take minimum of both
// 4. Apply hardware limitations

TickType_t xExpectedIdleTime = prvGetExpectedIdleTime();
// Example: Task A wakes in 500ms, Timer X expires in 300ms
// Result: Sleep for 300ms (until next event)
```

### 3. Core Sleep Implementation
```c
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime) {
    // Stop the tick interrupt
    portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;
    
    // Check if still safe to sleep (race condition protection)
    __disable_irq();
    if(eTaskConfirmSleepModeStatus() == eAbortSleep) {
        __enable_irq();
        return;  // Task became ready - abort sleep
    }
    
    // Configure wake timer
    ulReloadValue = ulTimerCountsForOneTick * xExpectedIdleTime;
    portNVIC_SYSTICK_LOAD_REG = ulReloadValue;
    
    // Enter sleep mode
    __WFI();  // Wait For Interrupt
    
    // Calculate actual sleep time and correct tick count
    ulCompletedTicks = (ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG) 
                       / ulTimerCountsForOneTick;
    vTaskStepTick(ulCompletedTicks);
    
    // Restart normal tick
    portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
}
```

## Implementation Patterns

### 1. Pre/Post Sleep Processing
```c
// Called BEFORE entering sleep
void vApplicationPreSleepProcessing(uint32_t *xExpectedIdleTime) {
    // Save peripheral states
    uart_saved_state = UART->CTRL;
    spi_saved_state = SPI->CONFIG;
    
    // Disable unnecessary peripherals
    UART->CTRL &= ~UART_ENABLE;
    SPI->CTRL &= ~SPI_ENABLE;
    ADC->CTRL &= ~ADC_ENABLE;
    
    // Configure wake sources
    EXTI->IMR |= WAKE_BUTTON_PIN;     // GPIO wake
    RTC->ALARM = *xExpectedIdleTime;   // Timer wake
    UART->CR3 |= UART_WUS_START_BIT;  // UART wake
    
    // Reduce clock frequency for ultra-low power
    if(*xExpectedIdleTime > pdMS_TO_TICKS(1000)) {
        RCC->CFGR |= RCC_CFGR_SW_MSI;  // Switch to MSI (low power)
    }
}

// Called AFTER waking from sleep
void vApplicationPostSleepProcessing(uint32_t xExpectedIdleTime) {
    // Restore clock frequency
    RCC->CFGR |= RCC_CFGR_SW_PLL;     // Switch back to PLL
    
    // Re-enable peripherals
    UART->CTRL = uart_saved_state;
    SPI->CONFIG = spi_saved_state;
    
    // Clear wake interrupt flags
    EXTI->PR = WAKE_BUTTON_PIN;
    RTC->ISR &= ~RTC_ALARM_FLAG;
    
    // Log wake source for debugging
    logWakeSource(getWakeSource());
}
```

### 2. Power State Management
```c
typedef enum {
    POWER_RUN,         // Full speed, all peripherals active
    POWER_SLEEP,       // CPU halt, peripherals running
    POWER_DEEP_SLEEP,  // CPU halt, most peripherals off
    POWER_STANDBY      // Everything off except RTC
} PowerState_t;

PowerState_t selectPowerState(TickType_t idleTime) {
    if(idleTime < pdMS_TO_TICKS(2)) {
        return POWER_RUN;        // Too short - stay awake
    }
    else if(idleTime < pdMS_TO_TICKS(10)) {
        return POWER_SLEEP;      // Light sleep
    }
    else if(idleTime < pdMS_TO_TICKS(1000)) {
        return POWER_DEEP_SLEEP; // Deep sleep
    }
    else {
        return POWER_STANDBY;    // Extended standby
    }
}

void enterPowerState(PowerState_t state) {
    switch(state) {
        case POWER_SLEEP:
            SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
            __WFI();
            break;
            
        case POWER_DEEP_SLEEP:
            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
            PWR->CR |= PWR_CR_LPDS;  // Low power deep sleep
            __WFI();
            break;
            
        case POWER_STANDBY:
            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
            PWR->CR |= PWR_CR_PDDS;  // Power down deep sleep
            __WFI();
            break;
    }
}
```

### 3. Wake Source Configuration
```c
typedef enum {
    WAKE_TIMER    = 0x01,  // RTC timer
    WAKE_UART_RX  = 0x02,  // UART receive
    WAKE_GPIO     = 0x04,  // Button/sensor
    WAKE_NETWORK  = 0x08,  // Network packet
    WAKE_USB      = 0x10   // USB activity
} WakeSource_t;

void configureWakeSources(uint32_t sources) {
    if(sources & WAKE_TIMER) {
        RTC->CR |= RTC_CR_ALRAE;      // Enable alarm
        EXTI->IMR |= EXTI_IMR_MR17;   // RTC alarm interrupt
    }
    
    if(sources & WAKE_UART_RX) {
        UART->CR3 |= UART_CR3_WUS_START;  // Wake on start bit
        UART->CR1 |= UART_CR1_UESM;       // Enable in stop mode
    }
    
    if(sources & WAKE_GPIO) {
        EXTI->IMR |= EXTI_IMR_MR0;    // Enable GPIO interrupt
        EXTI->FTSR |= EXTI_FTSR_TR0;  // Falling edge trigger
    }
}

WakeSource_t getWakeSource(void) {
    WakeSource_t source = 0;
    
    if(RTC->ISR & RTC_ISR_ALRAF) source |= WAKE_TIMER;
    if(UART->ISR & UART_ISR_WUF) source |= WAKE_UART_RX;
    if(EXTI->PR & EXTI_PR_PR0) source |= WAKE_GPIO;
    
    return source;
}
```

### 4. Power-Efficient Task Patterns
```c
// Pattern 1: Event-driven task (maximum sleep)
void vEventDrivenTask(void *pvParameters) {
    while(1) {
        // Wait indefinitely for events
        EventBits_t events = xEventGroupWaitBits(
            xEventGroup,
            ALL_EVENTS,
            pdTRUE,     // Clear on exit
            pdFALSE,    // OR condition
            portMAX_DELAY  // No timeout = deep sleep
        );
        
        // Handle events quickly and return to waiting
        handleEvents(events);
    }
}

// Pattern 2: Batch processing
void vBatchProcessingTask(void *pvParameters) {
    DataItem_t batch[BATCH_SIZE];
    uint32_t count = 0;
    
    while(1) {
        // Collect items with timeout
        if(xQueueReceive(xDataQueue, &batch[count], 
                        pdMS_TO_TICKS(100)) == pdTRUE) {
            count++;
            
            if(count >= BATCH_SIZE) {
                processBatch(batch, count);
                count = 0;
                // Long sleep after batch processing
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        } else {
            // Timeout - process partial batch
            if(count > 0) {
                processBatch(batch, count);
                count = 0;
            }
        }
    }
}
```

## Power Measurement and Optimization

### Current Consumption Tracking
```c
typedef struct {
    float voltage;              // Supply voltage
    float current_active_mA;    // Active mode current
    float current_sleep_mA;     // Sleep mode current
    uint32_t time_active_ms;    // Time in active mode
    uint32_t time_sleep_ms;     // Time in sleep mode
} PowerProfile_t;

float calculateAverageCurrent(PowerProfile_t *profile) {
    uint32_t total_time = profile->time_active_ms + profile->time_sleep_ms;
    if(total_time == 0) return 0;
    
    return (profile->current_active_mA * profile->time_active_ms +
            profile->current_sleep_mA * profile->time_sleep_ms) / total_time;
}

float estimateBatteryLife(float battery_mAh, PowerProfile_t *profile) {
    float avg_current = calculateAverageCurrent(profile);
    if(avg_current == 0) return INFINITY;
    
    // Apply 80% efficiency factor
    return (battery_mAh / avg_current) * 0.8;
}
```

### Dynamic Frequency Scaling
```c
typedef struct {
    uint32_t frequency_hz;
    uint32_t voltage_mv;
    float power_mA;
} FrequencyLevel_t;

FrequencyLevel_t powerLevels[] = {
    {200000000, 1200, 120.0},  // High performance
    {100000000, 1100, 60.0},   // Balanced
    {50000000,  1000, 30.0},   // Power saving
    {32768,     900,  0.5}     // Ultra low power
};

void adjustFrequencyForLoad(float cpuLoad) {
    FrequencyLevel_t *level;
    
    if(cpuLoad > 80.0) {
        level = &powerLevels[0];      // High performance
    } else if(cpuLoad > 50.0) {
        level = &powerLevels[1];      // Balanced
    } else if(cpuLoad > 20.0) {
        level = &powerLevels[2];      // Power saving
    } else {
        level = &powerLevels[3];      // Ultra low power
    }
    
    // Apply frequency and voltage
    setSystemClock(level->frequency_hz);
    setVoltageScaling(level->voltage_mv);
}
```

## Common Pitfalls

### Inefficient Sleep Patterns
```c
// BAD: Too frequent waking
void badTask(void) {
    while(1) {
        checkSensor();
        vTaskDelay(pdMS_TO_TICKS(10));  // Wake every 10ms!
    }
}

// GOOD: Batch operations, longer sleep
void goodTask(void) {
    while(1) {
        // Batch read all sensors at once
        readAllSensors();
        processAllSensorData();
        
        // Long sleep period
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second sleep
    }
}
```

### Peripheral Management Issues
```c
// BAD: Leaving peripherals enabled
void badSleepPrep(void) {
    ADC_Start();
    enterSleep();  // ADC still consuming power!
}

// GOOD: Disable unused peripherals
void goodSleepPrep(void) {
    ADC_Start();
    uint16_t reading = ADC_Read();
    ADC_Stop();     // Disable immediately
    enterSleep();   // Minimal power consumption
}
```

### Wake Source Configuration
```c
// BAD: No wake source configured
void badWakeSetup(void) {
    enterSleep();  // System may not wake up!
}

// GOOD: Configure multiple wake sources
void goodWakeSetup(void) {
    configureWakeSources(WAKE_TIMER | WAKE_UART_RX | WAKE_GPIO);
    enterSleep();  // Reliable wake-up guaranteed
}
```

## Integration with Wind Turbine System

### Power-Aware Task Scheduling
Our system optimizes power consumption while maintaining safety:

```c
// High-priority safety task - minimal power impact
void vSafetyTask(void *pvParameters) {
    while(1) {
        // Quick safety check
        if(checkCriticalConditions()) {
            triggerEmergencyStop();
        }
        
        // Sleep for 100ms (fast response, low power)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Low-priority sensor task - maximum power savings
void vSensorTask(void *pvParameters) {
    while(1) {
        // Batch read all sensors
        readVibrationSensors();
        readTemperatureSensors();
        readWindSpeed();
        
        // Process data
        updateSensorReadings();
        
        // Long sleep - 10 seconds (high power savings)
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
```

### Battery-Aware Power Profiles
```c
typedef struct {
    uint8_t batteryThreshold;
    uint32_t sensorPeriod_ms;
    uint32_t networkPeriod_ms;
    bool enableDeepSleep;
    const char *description;
} PowerProfile_t;

PowerProfile_t profiles[] = {
    {80, 1000,  5000,  false, "High Performance"},    // >80%
    {50, 2000,  10000, true,  "Balanced"},           // 50-80%
    {20, 5000,  30000, true,  "Power Saver"},        // 20-50%
    {10, 10000, 60000, true,  "Emergency"},          // 10-20%
    {0,  30000, 0,     true,  "Critical - Sensors Only"} // <10%
};

void adaptToBatteryLevel(uint8_t batteryPercent) {
    PowerProfile_t *profile = selectProfile(batteryPercent);
    
    // Update task periods
    updateSensorTaskPeriod(profile->sensorPeriod_ms);
    updateNetworkTaskPeriod(profile->networkPeriod_ms);
    
    // Enable/disable deep sleep
    configureDeepSleep(profile->enableDeepSleep);
    
    log_info("Power profile: %s", profile->description);
}
```

### Power Consumption Monitoring
The system tracks real-time power metrics:
- **Average current**: Weighted by time in each power state
- **Sleep efficiency**: Percentage of time in sleep modes
- **Battery life estimation**: Based on current consumption patterns
- **Power events**: Wake source tracking and frequency analysis

## Best Practices

- **Enable tickless idle**: Essential for significant power savings
- **Configure wake sources carefully**: Ensure system can always wake up
- **Batch operations**: Minimize wake frequency with longer processing periods
- **Use event-driven tasks**: Maximize sleep time with portMAX_DELAY
- **Monitor power consumption**: Track and optimize current usage
- **Scale clock frequency**: Match performance to workload requirements
- **Disable unused peripherals**: Every milliamp counts in battery systems
- **Test sleep/wake cycles**: Verify correct operation under all conditions