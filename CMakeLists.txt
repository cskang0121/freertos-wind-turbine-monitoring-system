cmake_minimum_required(VERSION 3.13)

# Project name and version
project(WindTurbinePredictor VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Options
option(SIMULATION_MODE "Build for simulation on host machine" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG_BUILD")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Platform-specific settings
if(SIMULATION_MODE)
    add_definitions(-DSIMULATION_MODE=1)
    message(STATUS "Building for SIMULATION mode")
    
    # For Mac/Linux simulation
    if(APPLE)
        message(STATUS "Building for macOS")
    elseif(UNIX)
        message(STATUS "Building for Linux")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    endif()
else()
    add_definitions(-DHARDWARE_MODE=1)
    message(STATUS "Building for HARDWARE mode (AmebaPro2)")
    # Hardware-specific flags would go here
endif()

# FreeRTOS configuration
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/FreeRTOS-Kernel")
set(FREERTOS_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config")

# Check if FreeRTOS kernel exists (will be added as submodule or downloaded)
if(NOT EXISTS "${FREERTOS_KERNEL_PATH}")
    message(STATUS "FreeRTOS kernel not found. Downloading...")
    
    # Download FreeRTOS kernel
    include(FetchContent)
    FetchContent_Declare(
        freertos_kernel
        GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
        GIT_TAG V10.4.3
        SOURCE_DIR ${FREERTOS_KERNEL_PATH}
    )
    FetchContent_MakeAvailable(freertos_kernel)
endif()

# FreeRTOS sources
set(FREERTOS_SOURCES
    ${FREERTOS_KERNEL_PATH}/tasks.c
    ${FREERTOS_KERNEL_PATH}/queue.c
    ${FREERTOS_KERNEL_PATH}/list.c
    ${FREERTOS_KERNEL_PATH}/timers.c
    ${FREERTOS_KERNEL_PATH}/event_groups.c
    ${FREERTOS_KERNEL_PATH}/stream_buffer.c
)

# Memory management (using heap_4)
list(APPEND FREERTOS_SOURCES
    ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c
)

# Port files based on platform
if(SIMULATION_MODE)
    # POSIX port for simulation
    list(APPEND FREERTOS_SOURCES
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/port.c
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
    )
    set(FREERTOS_PORT_INCLUDE 
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix/utils
    )
else()
    # ARM Cortex-M33 port for AmebaPro2
    list(APPEND FREERTOS_SOURCES
        ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
        ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
    )
    set(FREERTOS_PORT_INCLUDE 
        ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM33_NTZ/non_secure
    )
endif()

# Create FreeRTOS library
add_library(freertos STATIC ${FREERTOS_SOURCES})

target_include_directories(freertos PUBLIC
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_PORT_INCLUDE}
    ${FREERTOS_CONFIG_PATH}
)

# Platform-specific libraries
if(SIMULATION_MODE)
    target_link_libraries(freertos PUBLIC pthread)
endif()

# Main application sources
set(APP_SOURCES
    src/main.c
)

# Create main executable
add_executable(wind_turbine_predictor ${APP_SOURCES})

# Link with FreeRTOS
target_link_libraries(wind_turbine_predictor PRIVATE freertos)

# Include directories for main app
target_include_directories(wind_turbine_predictor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific
        target_link_libraries(wind_turbine_predictor PRIVATE m rt)
    endif()
endif()

# Build examples if requested
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build integrated system
add_subdirectory(src/integrated)

# Build tests if requested
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
install(TARGETS wind_turbine_predictor
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Simulation mode:   ${SIMULATION_MODE}")
message(STATUS "Build examples:    ${BUILD_EXAMPLES}")
message(STATUS "Build tests:       ${BUILD_TESTS}")
message(STATUS "C Compiler:        ${CMAKE_C_COMPILER}")
message(STATUS "C Flags:           ${CMAKE_C_FLAGS}")
message(STATUS "=============================")
message(STATUS "")