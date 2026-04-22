# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Navigation input filters (`CFG-NAVSPG-INFIL_*`): full library support
  for all six u-blox keys — `MINSVS`, `MAXSVS`, `MINCNO`, `MINELEV`
  (signed, degrees), `NCNOTHRS`, `CNOTHRS` — exposed through the C++
  `GnssConfig::NavigationFilters` optional struct, the C binding
  (`jp_gnss_navigation_filters_t`), and the Python `gnsshat` module
  (`navigation_filters` dict). Applied via VALSET/VALGET in the M9N,
  F9P and F10T startup paths with poll → set-mismatches → verify
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
- Visualization app: new **Altitude** tab — one-axis tape altimeter
  relative to a reference, with MSL / WGS84 source toggle that
  preserves the origin across conversions
- Visualization app: Configuration tab gained a **Navigation
  Filters** section with an Elevation Mask slider (0–60°)
- Visualization app: per-chart light / dark theme toggle (Relative
  Map, Altitude, Sky View, RF Analyzer) persisted in `localStorage`;
  dark is the default
- Visualization app: RF Analyzer no longer flickers "No RF data" when
  only one of `spectrum` / `rf_blocks` is present in a frame; spectrum
  x-axis tick density is now width-aware
- Visualization app: Relative Map and Altitude charts support mouse
  wheel zoom on hover plus +/- zoom buttons next to the range slider;
  grid ladder extended down to 1 cm for RTK-grade ranges (slider min
  lowered from 0.5 m to 0.05 m)
- Visualization app: Sky View and RF Analyzer theme toggles moved into
  a proper `.map-info` toolbar (matching the other tabs) instead of a
  floating overlay button

## [1.0.0] - 2026-04-06

Initial release — C++, C, and Python driver library for NEO-M9N, NEO-F10T,
and NEO-F9P GNSS HATs. Includes UBX protocol support, navigation, RTK,
geofencing, time base/mark, timepulse, gpsd forwarding, examples, and a
Flask visualization app.
