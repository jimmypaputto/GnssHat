cmake_minimum_required(VERSION 3.12)

project(TimeMark_C)

add_executable(time_mark src/time_mark.c)
set_target_properties(time_mark PROPERTIES OUTPUT_NAME time-mark)
target_link_libraries(time_mark GnssHat pthread)
