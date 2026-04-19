cmake_minimum_required(VERSION 3.12)

project(PrintNavigation_C)

add_executable(print_navigation src/print_navigation.c)
set_target_properties(print_navigation PROPERTIES OUTPUT_NAME print-navigation)
target_link_libraries(print_navigation GnssHat pthread)
