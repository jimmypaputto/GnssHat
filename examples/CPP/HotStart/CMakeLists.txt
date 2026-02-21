cmake_minimum_required(VERSION 3.10)

project(HotStart)

set(
    SOURCES
    src/HotStart.cpp
)

add_executable(HotStart ${SOURCES})

target_link_libraries(
    HotStart
    GnssHat
)
