cmake_minimum_required(VERSION 3.10)

project(PrintSatellites_C)

add_executable(print_satellites src/print_satellites.c)
target_link_libraries(print_satellites GnssHat pthread)
