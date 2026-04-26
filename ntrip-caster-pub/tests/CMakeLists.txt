# Jimmy Paputto 2026
#
# Tests for ntrip-caster-pub.  Enable via -DNTRIP_CASTER_TESTS=ON.

find_package(GTest REQUIRED)

add_executable(ntrip-caster-tests
    TestSatPos.cpp
    TestRtcmEphemeris.cpp
    TestRtcmAnalyzer.cpp
)
target_link_libraries(ntrip-caster-tests PRIVATE
    ntripcaster
    GTest::GTest
    GTest::Main)
target_include_directories(ntrip-caster-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

add_test(NAME ntrip-caster-tests COMMAND ntrip-caster-tests)
