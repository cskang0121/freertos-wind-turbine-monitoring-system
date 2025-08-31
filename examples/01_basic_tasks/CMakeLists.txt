cmake_minimum_required(VERSION 3.13)

# Example 01: Basic Tasks

# Create executable for the example
add_executable(basic_tasks_example main.c)

# Link with FreeRTOS
target_link_libraries(basic_tasks_example PRIVATE freertos)

# Include directories
target_include_directories(basic_tasks_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific
        target_link_libraries(basic_tasks_example PRIVATE m rt)
    endif()
endif()

# Installation
install(TARGETS basic_tasks_example
    RUNTIME DESTINATION bin/examples
)