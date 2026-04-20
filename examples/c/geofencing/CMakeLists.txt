cmake_minimum_required(VERSION 3.12)

project(Geofencing_C)

add_executable(geofencing src/geofencing.c)
target_link_libraries(geofencing GnssHat pthread)
