cmake_minimum_required(VERSION 3.12)

project(HotStart_C)

add_executable(hot_start src/hot_start.c)
set_target_properties(hot_start PROPERTIES OUTPUT_NAME hot-start)
target_link_libraries(hot_start GnssHat pthread)
