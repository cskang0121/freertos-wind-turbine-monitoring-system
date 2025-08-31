# Integrated Wind Turbine Monitor
cmake_minimum_required(VERSION 3.10)

# Create integrated executable
add_executable(turbine_monitor
    main.c
    tasks/sensor_task.c
    tasks/safety_task.c
    tasks/anomaly_task.c
    tasks/network_task.c
    tasks/dashboard_task.c
    dashboard/console.c
)

# Include directories
target_include_directories(turbine_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/config
    ${CMAKE_SOURCE_DIR}/external/FreeRTOS-Kernel/include
    ${CMAKE_SOURCE_DIR}/external/FreeRTOS-Kernel/portable/ThirdParty/GCC/Posix
)

# Link with FreeRTOS and math library
target_link_libraries(turbine_monitor
    freertos
    pthread
    m  # Math library for sqrt, sin, etc.
)

# Compiler flags
target_compile_options(turbine_monitor PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(turbine_monitor PRIVATE
        _DARWIN_C_SOURCE
    )
endif()

# Install target
install(TARGETS turbine_monitor
    RUNTIME DESTINATION bin
)