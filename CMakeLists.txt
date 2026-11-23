cmake_minimum_required(VERSION 3.24)

project(embedded_workshop LANGUAGES C CXX ASM)

if(NOT COMMAND configure_firmware_target)
    message(FATAL_ERROR "Configure with the project toolchain file")
endif()

include(cmake/dependencies.cmake)
add_subdirectory(third_party)
add_subdirectory(platform/stm32f407)

target_compile_definitions(freertos_plus_tcp_network_if PRIVATE
    STM32F4
    xPhyConfigure=stm32f407_xPhyConfigure
)
target_link_libraries(freertos_plus_tcp_network_if PUBLIC stm32f4_hal)
# V4.4.1 utility sources include FreeRTOS_IP.h without first including FreeRTOS.h.
target_compile_options(freertos_plus_tcp_utilities PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-include;FreeRTOS.h>
)

add_executable(${PROJECT_NAME}
    src/errors.cpp
    src/ethernet.cpp
    src/humidity_publisher.cpp
    src/leds_controller.cpp
    src/location_publisher.cpp
    src/leds_controller.cpp
    src/location_presets.cpp
    src/location_publisher.cpp
    src/main.cpp
    src/mqtt_client.cpp
    src/mqtt_transport.cpp
    src/tcp_echo_server.cpp
    src/temperature_publisher.cpp
    src/ui.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} PRIVATE
    freertos_kernel
    freertos_config
    freertos_plus_tcp
    freertos_plus_tcp_network_if
    core_mqtt_agent
    stm32f407_platform
)

set(LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f407/linker/STM32F407VGTX_FLASH.ld
)
configure_firmware_target(${PROJECT_NAME} ${LINKER_SCRIPT})
