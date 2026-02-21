cmake_minimum_required(VERSION 3.10)

project(RTK_C)

add_executable(rtk_base src/rtk_base.c)
target_link_libraries(rtk_base GnssHat pthread)

add_executable(rtk_rover src/rtk_rover.c)
target_link_libraries(rtk_rover GnssHat pthread)
