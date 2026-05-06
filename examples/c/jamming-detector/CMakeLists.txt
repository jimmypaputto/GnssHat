cmake_minimum_required(VERSION 3.12)

project(JammingDetector_C)

add_executable(jamming_detector src/jamming_detector.c)
set_target_properties(jamming_detector PROPERTIES OUTPUT_NAME jamming-detector)
target_link_libraries(jamming_detector GnssHat pthread)
