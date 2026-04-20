cmake_minimum_required(VERSION 3.12)

project(TimepulseInterrupt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
    SOURCES
    src/TimepulseInterrupt.cpp
)

add_executable(TimepulseInterrupt ${SOURCES})
set_target_properties(TimepulseInterrupt PROPERTIES OUTPUT_NAME timepulse-interrupt)

target_link_libraries(
    TimepulseInterrupt
    GnssHat
)
