cmake_minimum_required(VERSION 3.10)

project(PrintNavigation_C)

add_executable(print_navigation src/print_navigation.c)
target_link_libraries(print_navigation GnssHat pthread)
