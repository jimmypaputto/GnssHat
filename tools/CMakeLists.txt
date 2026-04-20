add_executable(gnsshat-info gnsshat-info.cpp)
target_include_directories(gnsshat-info PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gnsshat-info GnssHat)

# gnsshat-probe: links against the shared library
add_executable(gnsshat-probe gnsshat-probe.cpp)
target_include_directories(gnsshat-probe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gnsshat-probe GnssHat)

install(TARGETS gnsshat-info gnsshat-probe
    RUNTIME DESTINATION /usr/local/bin
)
