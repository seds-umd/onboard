cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(sos C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(sos sos.c)
target_link_libraries(sos pico_stdlib)
pico_add_extra_outputs(sos)      # this is what produces sos.uf2
add_executable(imu imu.c)
target_link_libraries(imu pico_stdlib hardware_i2c)
pico_enable_stdio_usb(imu 1)
pico_enable_stdio_uart(imu 0)
pico_add_extra_outputs(imu)
add_executable(blink blink.c)
target_link_libraries(blink pico_stdlib)
pico_add_extra_outputs(blink)
