cmake_minimum_required(VERSION 3.12)

project(TimeBase_C)

add_executable(time_base src/time_base.c)
target_link_libraries(time_base GnssHat pthread)
