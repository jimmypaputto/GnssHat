cmake_minimum_required(VERSION 3.10)

project(pure_c)

# set(CMAKE_C_STANDARD 11)

add_executable(${PROJECT_NAME} src/pure.c)
target_link_libraries(${PROJECT_NAME} GnssHat pthread)
