# Python Examples for GNSS HAT

This directory contains Python examples demonstrating how to use the GNSS HAT library.

## Prerequisites

Make sure the Python module is installed:
```bash
cd ../../python
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig
```

## Examples

### 1. print_navigation.py
Basic example showing how to:
- Configure the GNSS HAT with hardcoded settings
- Read navigation data continuously

```bash
python print_navigation.py
```

### 2. config_from_json.py
Advanced example demonstrating:
- Loading configuration from JSON files
- Supporting geofencing and timepulse configurations

```bash
python config_from_json.py res/config.json
python config_from_json.py res/config_no_geofencing.json
python config_from_json.py res/config_no_timepulse.json
```

### 3. geofencing.py
Geofencing example demonstrating:
- Setting up multiple geofences around specific locations
- Monitoring geofence status in real-time
- Understanding geofencing states and confidence levels

```bash
python geofencing.py
```

### 4. hot_start.py
Performance comparison example showing:
- Cold start vs hot start acquisition times
- Hard reset (cold start) that clears all satellite data
- Soft reset (hot start) that preserves satellite data
- Performance measurement and comparison

```bash
python hot_start.py
```

### 5. jamming_detector.py
RF interference and jamming detection example demonstrating:
- Real-time monitoring of RF blocks for interference
- Analysis of CW interference suppression levels
- Jamming state detection and classification
- Antenna status monitoring
- Detailed RF signal quality metrics

```bash
python jamming_detector.py
```

### 6. timepulse_interrupt.py
Timepulse interrupt example demonstrating:
- Configuring timepulse at a custom frequency
- Blocking wait for timepulse events
- Reading UTC time synchronized with timepulse

```bash
python timepulse_interrupt.py
```

### 7. rtk_base.py
RTK Base Station example demonstrating:
- Reading RTCM3 correction frames from a base station
- Parsing RTCM3 message IDs
- Monitoring fix type for RTK readiness

```bash
python rtk_base.py
```

### 8. rtk_rover.py
RTK Rover example demonstrating:
- Monitoring RTK fix quality on a rover
- Applying RTCM3 corrections
- Tracking fix quality and fix type

```bash
python rtk_rover.py
```

### 9. print_satellites.py
Satellite visibility and signal quality example demonstrating:
- Per-satellite GNSS data from UBX-NAV-SAT
- Constellation identification (GPS, Galileo, GLONASS, BeiDou, SBAS, QZSS)
- Signal quality, C/N0, elevation, azimuth per satellite
- Used-in-fix and health status
- Ephemeris, almanac, and DGPS availability counters

```bash
python print_satellites.py
```
