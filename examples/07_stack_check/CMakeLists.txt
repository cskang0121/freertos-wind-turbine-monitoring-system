cmake_minimum_required(VERSION 3.13)

# Example 07: Stack Check

# Create executable for the example
add_executable(stack_check_example main.c)

# Link with FreeRTOS and math library
target_link_libraries(stack_check_example PRIVATE freertos m)

# Include directories
target_include_directories(stack_check_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific  
        target_link_libraries(stack_check_example PRIVATE rt)
    endif()
endif()

# Installation
install(TARGETS stack_check_example
    RUNTIME DESTINATION bin/examples
)