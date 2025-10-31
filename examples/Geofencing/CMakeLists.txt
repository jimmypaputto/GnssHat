cmake_minimum_required(VERSION 3.10)

project(Geofencing)

set(
    SOURCES
    src/Geofencing.cpp
)

add_executable(Geofencing ${SOURCES})

target_link_libraries(
    Geofencing
    GnssHat
)
