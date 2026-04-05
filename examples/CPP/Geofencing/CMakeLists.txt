cmake_minimum_required(VERSION 3.12)

project(Geofencing)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(
    SOURCES
    src/Geofencing.cpp
)

add_executable(Geofencing ${SOURCES})

target_link_libraries(
    Geofencing
    GnssHat
)
