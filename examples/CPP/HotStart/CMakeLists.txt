cmake_minimum_required(VERSION 3.12)

project(HotStart)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
    SOURCES
    src/HotStart.cpp
)

add_executable(HotStart ${SOURCES})

target_link_libraries(
    HotStart
    GnssHat
)
