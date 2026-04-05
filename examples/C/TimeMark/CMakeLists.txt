cmake_minimum_required(VERSION 3.12)

project(TimeMark_C)

add_executable(time_mark src/time_mark.c)
target_link_libraries(time_mark GnssHat pthread)
