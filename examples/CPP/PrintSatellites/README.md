# PrintSatellites Example

Satellite visibility and signal quality display examples demonstrating per-satellite GNSS data from the UBX-NAV-SAT message.

## Overview

This example provides two different approaches to displaying satellite information:

- **PrintSatellitesBasic**: Simple text output listing each tracked satellite
- **PrintSatellitesAdvanced**: Rich terminal UI with color-coded constellations, signal strength bars, and constellation summaries

Both examples read the `Navigation::satellites` vector populated by the UBX-NAV-SAT (0x01 0x35) message.

## Features

### PrintSatellitesBasic
- Simple tabular satellite listing
- Per-satellite: GNSS system, SV ID, C/N0, elevation, azimuth, quality, usage, health
- Total and used-in-fix satellite counts
- Lightweight implementation

### PrintSatellitesAdvanced
- Rich terminal user interface with ANSI colors and Unicode signal bars
- Color-coded constellations (GPS, Galileo, GLONASS, BeiDou, SBAS, QZSS)
- Signal strength visualization with bar indicators
- Constellation summary with per-system used/total counts
- Sorted display: used-in-fix satellites first, then by constellation and signal strength
- Ephemeris, almanac, and DGPS correction availability counters
- Real-time 1 Hz refresh with screen clearing

## Displayed Data

| Field       | Description                                      |
|-------------|--------------------------------------------------|
| GNSS System | GPS, Galileo, GLONASS, BeiDou, SBAS, QZSS, IMES  |
| SV ID       | Satellite vehicle identifier                     |
| C/N0        | Carrier-to-noise ratio (dBHz)                    |
| Elevation   | Elevation angle above horizon (degrees)          |
| Azimuth     | Azimuth angle from north (degrees)               |
| Quality     | Signal quality indicator                         |
| Used in Fix | Whether satellite is used in position solution   |
| Health      | Satellite health status                          |

## Build Instructions

```bash
# From the PrintSatellites example directory
mkdir -p build
cd build
cmake ..
make
```

This creates two executables:
- `PrintSatellitesBasic`
- `PrintSatellitesAdvanced`

## Usage

### Basic Satellite Display
```bash
./PrintSatellitesBasic
```

### Advanced Satellite Display
```bash
./PrintSatellitesAdvanced
```

Press `Ctrl+C` to exit either program.
