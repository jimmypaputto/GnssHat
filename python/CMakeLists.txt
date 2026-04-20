# Jimmy Paputto 2025

cmake_minimum_required(VERSION 3.12)

project(jimmypaputto_gnss_python)

# Find Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

message(STATUS "Building Python extension for Jimmy Paputto GNSS HAT")
message(STATUS "Python: ${Python3_VERSION}")

# Create Python extension
Python3_add_library(gnsshat MODULE src/jp_gnss_module.c)

target_compile_definitions(gnsshat PRIVATE PY_SSIZE_T_CLEAN)

if(TARGET GnssHat)
    # Built as subdirectory of the main project — link directly to build target
    message(STATUS "Library: GnssHat (build target)")
    target_include_directories(gnsshat PRIVATE
        ${Python3_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(gnsshat PRIVATE
        GnssHat
        ${Python3_LIBRARIES}
    )
else()
    # Standalone build — find installed library
    find_library(GNSS_LIBRARY gnsshat PATHS /usr/local/lib REQUIRED)
    find_path(GNSS_INCLUDE_DIR GnssHat.h PATHS /usr/local/include/jimmypaputto REQUIRED)
    message(STATUS "Library: ${GNSS_LIBRARY}")
    target_include_directories(gnsshat PRIVATE
        ${Python3_INCLUDE_DIRS}
        ${GNSS_INCLUDE_DIR}
    )
    target_link_libraries(gnsshat PRIVATE
        ${GNSS_LIBRARY}
        ${Python3_LIBRARIES}
    )
endif()

# System-wide installation (consistent with C library in /usr/local/lib)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import sysconfig; print(sysconfig.get_path('platlib', vars={'base': '/usr', 'platbase': '/usr'}))"
    OUTPUT_VARIABLE PYTHON_INSTALL_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS gnsshat LIBRARY DESTINATION ${PYTHON_INSTALL_DIR}/jimmypaputto)
install(FILES jimmypaputto_init.py DESTINATION ${PYTHON_INSTALL_DIR}/jimmypaputto RENAME __init__.py)

message(STATUS "Will install to: ${PYTHON_INSTALL_DIR}")
