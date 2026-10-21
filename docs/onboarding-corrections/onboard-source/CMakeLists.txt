set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-arm-s)
cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


project(pico_sensors C CXX ASM)


pico_sdk_init()

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(pico_sensors
    src/imu.c
    src/mag.c
    src/baro.c
    src/main.c
)

target_link_libraries(pico_sensors
    pico_stdlib
    hardware_i2c
)

pico_enable_stdio_usb(pico_sensors 1)
pico_enable_stdio_uart(pico_sensors 0)

pico_add_extra_outputs(pico_sensors)
