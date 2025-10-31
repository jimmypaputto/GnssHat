# GNSS HAT - Python Interface

Python bindings for the GNSS HAT C++ Library.

## Features

- **Full Object-Oriented Interface** - Clean, Pythonic API with classes and methods  
- **Real-time Navigation Data** - Position, velocity, time with high precision  
- **GPSD Integration** - Seamless integration with GPSD ecosystem  
- **Timepulse Support** - Precision timing for synchronization applications  
- **High Performance** - Direct C bindings for minimal overhead  

## Installation

### Prerequisites

```bash
# Install development tools
sudo apt-get update
sudo apt-get install build-essential python3-dev python3-setuptools

# Install Jimmy Paputto GNSS HAT C library first
cd GnssHat
mkdir -p build && cd build
cmake ..
make
sudo make install
sudo ldconfig
```

### Build and Install Python Interface

```bash
cd GnssHat/python
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig
```

## Quick Start

```python
import jimmypaputto_gnss as gnss

# Simple usage with context manager
with gnss.GnssHat() as hat:
    # Configure and start
    config = {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DYNAMIC_MODEL_PORTABLE,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TIMEPULSE_POLARITY_RISING_EDGE
        },
        'geofencing': None
    }
    
    hat.start(config)
    
    # Get navigation data
    nav = hat.wait_and_get_fresh_navigation()
    pvt = nav.pvt

    print(f"Position: {pvt.latitude:.6f}, {pvt.longitude:.6f}")
    print(f"Altitude: {pvt.altitude:.1f}m")
    print(f"Satellites: {pvt.visible_satellites}")
    print(f"Accuracy: {pvt.horizontal_accuracy:.1f}m")
```

## API Reference

### GnssHat Class

Main interface to the GNSS HAT hardware.

#### Methods

- `start(config)` - Start GNSS with optional configuration
- `get_navigation()` - Get current navigation data (non-blocking)
- `wait_and_get_fresh_navigation()` - Wait for fresh navigation data (blocking)
- `enable_timepulse()` - Enable 1PPS timepulse output
- `disable_timepulse()` - Disable timepulse output
- `start_gpsd_forwarding()` - Start NMEA forwarding to GPSD
- `stop_gpsd_forwarding()` - Stop GPSD forwarding
- `get_gpsd_device_path()` - Get virtual device path for GPSD
- `hard_reset()` - Perform hardware reset (cold start)
- `soft_reset()` - Perform software reset (hot start)

### Navigation Class

Complete navigation information container.

#### Properties

- `pvt` - PositionVelocityTime object
- `dop` - Dilution of Precision information
- `geofencing` - Geofencing status and configuration
- `rf_blocks` - RF interference and antenna status

### Constants

#### Dynamic Models
- `DYNAMIC_MODEL_PORTABLE` - General purpose
- `DYNAMIC_MODEL_STATIONARY` - Stationary applications
- `DYNAMIC_MODEL_PEDESTRIAN` - Walking/hiking
- `DYNAMIC_MODEL_AUTOMOTIVE` - Car/truck
- `DYNAMIC_MODEL_SEA` - Marine applications
- `DYNAMIC_MODEL_AIRBORNE_1G` - Aircraft < 1g acceleration
- `DYNAMIC_MODEL_AIRBORNE_2G` - Aircraft < 2g acceleration
- `DYNAMIC_MODEL_AIRBORNE_4G` - Aircraft < 4g acceleration
- `DYNAMIC_MODEL_WRIST` - Wrist-worn devices
- `DYNAMIC_MODEL_BIKE` - Bicycle
- `DYNAMIC_MODEL_MOWER` - Robotic mower
- `DYNAMIC_MODEL_ESCOOTER` - Electric scooter

#### Timepulse Polarity
- `TIMEPULSE_POLARITY_FALLING_EDGE`
- `TIMEPULSE_POLARITY_RISING_EDGE`

#### Fix Quality
- `FIX_QUALITY_INVALID` - No fix available
- `FIX_QUALITY_GPS_FIX_2D_3D` - Standard GPS fix
- `FIX_QUALITY_DGNSS` - Differential GPS fix
- `FIX_QUALITY_PPS_FIX` - PPS fix
- `FIX_QUALITY_FIXED_RTK` - RTK fixed solution
- `FIX_QUALITY_FLOAT_RTK` - RTK float solution
- `FIX_QUALITY_DEAD_RECKONING` - dead reckoning
