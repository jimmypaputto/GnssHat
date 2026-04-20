cmake_minimum_required(VERSION 3.12)

project(PrintSatellites_C)

add_executable(print_satellites src/print_satellites.c)
set_target_properties(print_satellites PROPERTIES OUTPUT_NAME print-satellites)
target_link_libraries(print_satellites GnssHat pthread)
