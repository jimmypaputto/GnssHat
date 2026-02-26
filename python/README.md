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
from jimmypaputto import gnsshat

# Simple usage with context manager
with gnsshat.GnssHat() as hat:
    # Configure and start
    config = {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.PORTABLE,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
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

- `start(config)` - Start GNSS with configuration dict
- `get_navigation()` - Get current navigation data (non-blocking)
- `wait_and_get_fresh_navigation()` - Wait for fresh navigation data (blocking)
- `enable_timepulse()` - Enable 1PPS timepulse output
- `disable_timepulse()` - Disable timepulse output
- `timepulse()` - Block until next timepulse interrupt (call `enable_timepulse()` first)
- `start_forward_for_gpsd()` - Start NMEA forwarding to GPSD
- `stop_forward_for_gpsd()` - Stop GPSD forwarding
- `join_forward_for_gpsd()` - Join (wait for) GPSD forwarding thread
- `get_gpsd_device_path()` - Get virtual device path for GPSD
- `hard_reset_cold_start()` - Perform hardware reset (cold start)
- `soft_reset_hot_start()` - Perform software reset (hot start)
- `rtk_get_full_corrections()` - Get full RTK base corrections (RTCM3 frames)
- `rtk_get_tiny_corrections()` - Get tiny RTK base corrections (RTCM3 frames)
- `rtk_get_rtcm3_frame(msg_id)` - Get a specific RTCM3 frame by message ID
- `rtk_apply_corrections(frames)` - Apply RTK corrections to a rover

### Navigation Class

Complete navigation information container.

#### Properties

- `pvt` - PositionVelocityTime object
- `dop` - Dilution of Precision information
- `geofencing` - Geofencing status and configuration
- `rf_blocks` - RF interference and antenna status

### IntEnum Constants

All enum-like values are exposed as Python `IntEnum` types. Use them via `gnsshat.<EnumName>.<MEMBER>`.

#### DynamicModel
- `DynamicModel.PORTABLE` - General purpose
- `DynamicModel.STATIONARY` - Stationary applications
- `DynamicModel.PEDESTRIAN` - Walking/hiking
- `DynamicModel.AUTOMOTIVE` - Car/truck
- `DynamicModel.SEA` - Marine applications
- `DynamicModel.AIRBORNE_1G` - Aircraft < 1g acceleration
- `DynamicModel.AIRBORNE_2G` - Aircraft < 2g acceleration
- `DynamicModel.AIRBORNE_4G` - Aircraft < 4g acceleration
- `DynamicModel.WRIST` - Wrist-worn devices
- `DynamicModel.BIKE` - Bicycle
- `DynamicModel.MOWER` - Robotic mower
- `DynamicModel.ESCOOTER` - Electric scooter

#### TimepulsePolarity
- `TimepulsePolarity.FALLING_EDGE`
- `TimepulsePolarity.RISING_EDGE`

#### FixQuality
- `FixQuality.INVALID` - No fix available
- `FixQuality.GPS_FIX_2D_3D` - Standard GPS fix
- `FixQuality.DGNSS` - Differential GPS fix
- `FixQuality.PPS_FIX` - PPS fix
- `FixQuality.FIXED_RTK` - RTK fixed solution
- `FixQuality.FLOAT_RTK` - RTK float solution
- `FixQuality.DEAD_RECKONING` - Dead reckoning

#### FixStatus
- `FixStatus.VOID` - No fix
- `FixStatus.ACTIVE` - Active fix

#### FixType
- `FixType.NO_FIX` - No position fix
- `FixType.DEAD_RECKONING_ONLY` - Dead reckoning only
- `FixType.FIX_2D` - 2D fix
- `FixType.FIX_3D` - 3D fix
- `FixType.GNSS_WITH_DEAD_RECKONING` - GNSS + dead reckoning
- `FixType.TIME_ONLY_FIX` - Time-only fix

#### GeofenceStatus / GeofencingStatus
- `GeofenceStatus.UNKNOWN`, `GeofenceStatus.INSIDE`, `GeofenceStatus.OUTSIDE`
- `GeofencingStatus.NOT_AVAILABLE`, `GeofencingStatus.ACTIVE`

#### RfBand / JammingState / AntennaStatus / AntennaPower
- `RfBand.L1`, `RfBand.L2_OR_L5`
- `JammingState.UNKNOWN`, `JammingState.OK_NO_SIGNIFICANT_JAMMING`, `JammingState.WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK`, `JammingState.CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX`
- `AntennaStatus.INIT`, `AntennaStatus.DONT_KNOW`, `AntennaStatus.OK`, `AntennaStatus.SHORT`, `AntennaStatus.OPEN`
- `AntennaPower.OFF`, `AntennaPower.ON`, `AntennaPower.DONT_KNOW`

#### RTK Enums
- `RtkMode.BASE`, `RtkMode.ROVER`
- `BaseMode.SURVEY_IN`, `BaseMode.FIXED_POSITION`
- `FixedPositionType.ECEF`, `FixedPositionType.LLA`

#### GnssId / SvQuality
- `GnssId.GPS`, `GnssId.SBAS`, `GnssId.GALILEO`, `GnssId.BEIDOU`, `GnssId.IMES`, `GnssId.QZSS`, `GnssId.GLONASS`
- `SvQuality.NO_SIGNAL`, `SvQuality.SEARCHING`, `SvQuality.SIGNAL_ACQUIRED`, `SvQuality.SIGNAL_DETECTED_BUT_UNUSABLE`, `SvQuality.CODE_LOCKED_AND_TIME_SYNCHRONIZED`, `SvQuality.CODE_AND_CARRIER_LOCKED_1`, `SvQuality.CODE_AND_CARRIER_LOCKED_2`, `SvQuality.CODE_AND_CARRIER_LOCKED_3`
