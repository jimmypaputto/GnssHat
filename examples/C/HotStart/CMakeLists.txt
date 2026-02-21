cmake_minimum_required(VERSION 3.10)

project(HotStart_C)

add_executable(hot_start src/hot_start.c)
target_link_libraries(hot_start GnssHat pthread)
