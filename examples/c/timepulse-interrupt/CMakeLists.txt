cmake_minimum_required(VERSION 3.12)

project(TimepulseInterrupt_C)

add_executable(timepulse_interrupt src/timepulse_interrupt.c)
set_target_properties(timepulse_interrupt PROPERTIES OUTPUT_NAME timepulse-interrupt)
target_link_libraries(timepulse_interrupt GnssHat pthread)
