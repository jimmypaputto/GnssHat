cmake_minimum_required(VERSION 3.12)

project(RTK_C)

add_executable(rtk_base src/rtk_base.c)
set_target_properties(rtk_base PROPERTIES OUTPUT_NAME rtk-base)
target_link_libraries(rtk_base GnssHat pthread)

add_executable(rtk_rover src/rtk_rover.c)
set_target_properties(rtk_rover PROPERTIES OUTPUT_NAME rtk-rover)
target_link_libraries(rtk_rover GnssHat pthread)
