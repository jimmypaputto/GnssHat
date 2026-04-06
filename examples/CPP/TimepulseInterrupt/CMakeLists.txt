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

target_link_libraries(
    TimepulseInterrupt
    GnssHat
)
