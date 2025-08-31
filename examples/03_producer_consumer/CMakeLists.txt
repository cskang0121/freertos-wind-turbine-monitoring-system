cmake_minimum_required(VERSION 3.13)

# Example 03: Producer-Consumer

# Create executable for the example
add_executable(producer_consumer_example main.c)

# Link with FreeRTOS and math library
target_link_libraries(producer_consumer_example PRIVATE freertos m)

# Include directories
target_include_directories(producer_consumer_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific  
        target_link_libraries(producer_consumer_example PRIVATE rt)
    endif()
endif()

# Installation
install(TARGETS producer_consumer_example
    RUNTIME DESTINATION bin/examples
)