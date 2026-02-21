cmake_minimum_required(VERSION 3.10)

project(TimepulseInterrupt_C)

add_executable(timepulse_interrupt src/timepulse_interrupt.c)
target_link_libraries(timepulse_interrupt GnssHat pthread)
