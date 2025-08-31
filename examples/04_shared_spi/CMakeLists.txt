cmake_minimum_required(VERSION 3.13)

# Example 04: Shared SPI with Mutex

# Create executable for the example
add_executable(shared_spi_example main.c)

# Link with FreeRTOS and math library
target_link_libraries(shared_spi_example PRIVATE freertos m)

# Include directories
target_include_directories(shared_spi_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

# Platform-specific linking
if(SIMULATION_MODE)
    if(APPLE)
        # macOS specific
    elseif(UNIX)
        # Linux specific  
        target_link_libraries(shared_spi_example PRIVATE rt)
    endif()
endif()

# Installation
install(TARGETS shared_spi_example
    RUNTIME DESTINATION bin/examples
)