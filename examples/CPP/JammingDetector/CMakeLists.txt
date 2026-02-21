cmake_minimum_required(VERSION 3.10)

project(JammingDetector)

set(
    SOURCES
    src/JammingDetector.cpp
)

add_executable(JammingDetector ${SOURCES})

target_link_libraries(
    JammingDetector
    GnssHat
)
