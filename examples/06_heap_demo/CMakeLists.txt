cmake_minimum_required(VERSION 3.13)

# Example 06: Heap Demo

# Create executable for the example
add_executable(heap_demo_example main.c)

# Link with FreeRTOS and math library
target_link_libraries(heap_demo_example PRIVATE freertos m)

# Include directories
target_include_directories(heap_demo_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific  
        target_link_libraries(heap_demo_example PRIVATE rt)
    endif()
endif()

# Installation
install(TARGETS heap_demo_example
    RUNTIME DESTINATION bin/examples
)