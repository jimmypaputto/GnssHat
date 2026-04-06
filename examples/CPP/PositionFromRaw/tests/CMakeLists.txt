cmake_minimum_required(VERSION 3.10)

project(PositionFromRawTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(GTest REQUIRED)

# Header-only algorithm files live in ../src
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(AlgorithmTests
    TestCoordinates.cpp
    TestSppSolver.cpp
    TestCarrierPhase.cpp
    TestAmbiguityFloat.cpp
    TestAmbiguityLambda.cpp
    TestDualFrequency.cpp
    TestAtmosphere.cpp
    MockData.hpp
)

target_link_libraries(AlgorithmTests GTest::GTest GTest::Main GnssHat pthread)

enable_testing()
add_test(NAME AlgorithmTests COMMAND AlgorithmTests)
