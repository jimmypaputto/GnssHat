# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- NTRIP caster, client, and server with optional TLS
- CLI tools: `gnsshat-info`, `gnsshat-probe`
- CMake config package (`find_package(GnssHat)`) and pkg-config
- `BUILD_TESTS` option — tests integrate into root build via `ctest`
- CMake `uninstall` target
- rebuild.sh script for frequent clean/purge/install/test operations 

### Changed
- Single-source version via `Version.hpp.in`
- Install paths use `GNUInstallDirs`
- Tests link against shared library instead of recompiling sources

## [1.0.0] - 2026-04-06

Initial release — C++, C, and Python driver library for NEO-M9N, NEO-F10T,
and NEO-F9P GNSS HATs. Includes UBX protocol support, navigation, RTK,
geofencing, time base/mark, timepulse, gpsd forwarding, examples, and a
Flask visualization app.
