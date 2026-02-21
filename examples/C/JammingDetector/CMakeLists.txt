cmake_minimum_required(VERSION 3.10)

project(JammingDetector_C)

add_executable(jamming_detector src/jamming_detector.c)
target_link_libraries(jamming_detector GnssHat pthread)
