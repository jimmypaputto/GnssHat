# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- NTRIP caster, client, and server with optional TLS
- CLI tools: `gnsshat-info`, `gnsshat-probe`
- `gnsshat-rtk-base` — RTK base station tool with TOML config and a
  systemd service unit (`tools/res/gnsshat-rtk-base.service`) plus
  install/uninstall scripts under `tools/scripts/`
- Public `JimmyPaputto::Hat::detectProduct()` and
  `JimmyPaputto::Hat::readEepromField()` helpers for reading the HAT
  EEPROM device-tree entries without instantiating `IGnssHat`
- CMake config package (`find_package(GnssHat)`) and pkg-config
- `BUILD_TESTS` option — tests integrate into root build via `ctest`
- CMake `uninstall` target
- rebuild.sh script for frequent clean/purge/install/test operations 

### Changed
- Single-source version via `Version.hpp.in`
- Install paths use `GNUInstallDirs`
- Tests link against shared library instead of recompiling sources
- `IGnssHat::create()` and `gnsshat-probe` now reuse the shared
  `JimmyPaputto::Hat` helpers instead of duplicating device-tree parsers
- `gnsshat-rtk-base.service` unit: `Type=notify`, `NotifyAccess=main`,
  `WatchdogSec=30`, `SyslogLevelPrefix=true`, journald rate-limit
  fallback (`LogRateLimitIntervalSec=30`, `LogRateLimitBurst=200`),
  `StartLimit*` moved to `[Unit]` where they actually take effect

## [1.0.0] - 2026-04-06

Initial release — C++, C, and Python driver library for NEO-M9N, NEO-F10T,
and NEO-F9P GNSS HATs. Includes UBX protocol support, navigation, RTK,
geofencing, time base/mark, timepulse, gpsd forwarding, examples, and a
Flask visualization app.
