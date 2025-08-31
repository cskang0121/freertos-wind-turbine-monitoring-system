cmake_minimum_required(VERSION 3.13)

# Example 08: Power Management

# Create executable for the example
add_executable(power_save_example main.c)

# Link with FreeRTOS and math library
target_link_libraries(power_save_example PRIVATE freertos m)

# Include directories
target_include_directories(power_save_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific  
        target_link_libraries(power_save_example PRIVATE rt)
    endif()
endif()

# Installation
install(TARGETS power_save_example
    RUNTIME DESTINATION bin/examples
)