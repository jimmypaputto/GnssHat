# Jimmy Paputto 2026

if(NOT TARGET GnssHat)
    message(FATAL_ERROR
        "tests/ must be built via the root CMakeLists.txt with -DBUILD_TESTS=ON.\n"
        "Run: cmake .. -DBUILD_TESTS=ON"
    )
endif()

find_package(GTest REQUIRED)

add_executable(GnssHatTests
    TestUtils.cpp
    TestUbxChecksum.cpp
    TestGnssConfig.cpp
    TestUbxMessages.cpp
    TestRtcm3.cpp
    TestUbxClassMsgId.cpp
    TestNtrip.cpp
)
set_target_properties(GnssHatTests PROPERTIES OUTPUT_NAME gnsshat-tests)

target_include_directories(GnssHatTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(GnssHatTests GnssHat GTest::GTest GTest::Main)

add_test(NAME GnssHatTests COMMAND GnssHatTests)
