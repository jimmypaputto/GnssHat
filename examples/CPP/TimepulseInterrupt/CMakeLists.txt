cmake_minimum_required(VERSION 3.10)

project(TimepulseInterrupt)

set(
    SOURCES
    src/TimepulseInterrupt.cpp
)

add_executable(TimepulseInterrupt ${SOURCES})

target_link_libraries(
    TimepulseInterrupt
    GnssHat
)
