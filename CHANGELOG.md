# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-05-06

### Added
- Navigation input filters (experimental)
- NTRIP caster, client, and server with optional TLS
- CLI tools: `gnsshat-info`, `gnsshat-probe`
- `gnsshat-rtk-base` - RTK base station tool with TOML config and a systemd service unit
- Helpers for reading the HAT EEPROM device-tree entries without instantiating `IGnssHat` - JimmyPaputto::Hat namespace
- Many CMake fixes including Version.hpp generation for single version source
- CI workflows for self hosted services including RPI4, RPI5 and x64 machine for more frequent checks

### Changed
- Tests link against shared library instead of recompiling sources
- Visualization app: new **Altitude** tab - one-axis tape altimeter
- Visualization app: Configuration tab gained a **NavigationFilters** section with an Elevation Mask slider (0–60°)
- Visualization app: per-chart light / dark theme toggle
- Visualization app: RF Analyzer no longer flickers "No RF data" when only one of `spectrum` / `rf_blocks` is present in a frame; spectrum x-axis tick density is now width-aware
- Visualization app: Relative Map and Altitude charts support mouse wheel zoom on hover plus +/- zoom buttons next to the range slider; 
- Visualization app: Grid ladder extended down to 1 cm for RTK-grade ranges
- Visualization app: Sky View and RF Analyzer theme toggles moved intoa proper `.map-info` toolbar matching the other tabs

## [1.0.0] - 2026-04-06

Initial release — C++, C, and Python driver library for NEO-M9N, NEO-F10T,
and NEO-F9P GNSS HATs. Includes UBX protocol support, navigation, RTK,
geofencing, time base/mark, timepulse, gpsd forwarding, examples, and a
Flask visualization app.
