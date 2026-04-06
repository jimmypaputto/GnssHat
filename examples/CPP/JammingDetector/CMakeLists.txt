cmake_minimum_required(VERSION 3.12)

project(JammingDetector)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
    SOURCES
    src/JammingDetector.cpp
)

add_executable(JammingDetector ${SOURCES})

target_link_libraries(
    JammingDetector
    GnssHat
)
