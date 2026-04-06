# GNSS HAT - Python Bindings

CPython extension module for the Jimmy Paputto GNSS HAT library. Wraps the C API (`libGnssHat.so`) as a native Python module.

```python
from jimmypaputto import gnsshat

hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
hat.start({
    'measurement_rate_hz': 1,
    'dynamic_model': gnsshat.DynamicModel.PORTABLE,
    'timepulse_pin_config': {
        'active': True,
        'fixed_pulse': {'frequency': 1, 'pulse_width': 0.1},
        'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
    }
})

nav = hat.wait_and_get_fresh_navigation()
print(f"{nav.pvt.latitude:.6f}, {nav.pvt.longitude:.6f}")
```

## Installation

```bash
sudo apt install build-essential cmake libgpiod-dev python3-dev

cd GnssHat
mkdir -p build && cd build
cmake .. -DBUILD_PYTHON=ON
make -j$(nproc)
sudo make install && sudo ldconfig
```

The Python module can also be built standalone (requires the C++ library installed first):

```bash
cd GnssHat/python
mkdir -p build && cd build
cmake .. && make -j$(nproc)
sudo make install
```

Installs to `~/.local/lib/pythonX.Y/site-packages/jimmypaputto/`.

## Configuration

The `start()` method accepts a Python dict. All fields except `measurement_rate_hz`, `dynamic_model`, and `timepulse_pin_config` are optional.

```python
config = {
    'measurement_rate_hz': 10,                    # 1-25 Hz
    'dynamic_model': gnsshat.DynamicModel.AUTOMOTIVE,
    'timepulse_pin_config': {
        'active': True,
        'fixed_pulse': {'frequency': 1, 'pulse_width': 0.1},
        'pulse_when_no_fix': {'frequency': 1, 'pulse_width': 0.5},  # optional
        'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
    },

    # Geofencing (L1 HAT, RTK HAT only) — None or omit to disable
    'geofencing': {
        'confidence_level': 3,          # 0-5
        'pin_polarity': gnsshat.PioPinPolarity.LOW_MEANS_INSIDE,  # optional
        'geofences': [
            {'lat': 52.2297, 'lon': 21.0122, 'radius': 100.0},   # max 4
        ]
    },

    # RTK (RTK HAT only) — None or omit to disable
    'rtk': {
        'mode': gnsshat.RtkMode.BASE,
        'base': {
            'base_mode': gnsshat.BaseMode.SURVEY_IN,
            'survey_in': {
                'minimum_observation_time_s': 60,
                'required_position_accuracy_m': 2.0
            }
            # OR for fixed position:
            # 'base_mode': gnsshat.BaseMode.FIXED_POSITION,
            # 'fixed_position': {
            #     'position_type': gnsshat.FixedPositionType.LLA,
            #     'lla': {'latitude_deg': 52.2, 'longitude_deg': 21.0, 'height_m': 120.0},
            #     'position_accuracy_m': 0.01
            # }
        }
    },

    # Timing (TIME HAT only) — None or omit to disable
    'timing': {
        'enable_time_mark': False,           # Enable UBX-TIM-TM2 time mark events
        'time_base': {                       # Optional — Survey-In or Fixed Position for improved time accuracy
            'base_mode': gnsshat.BaseMode.SURVEY_IN,
            'survey_in': {
                'minimum_observation_time_s': 60,
                'required_position_accuracy_m': 5.0
            }
        }
    }
}
```

## GnssHat Methods

| Method | Description |
|--------|-------------|
| `start(config)` | Configure and start the GNSS module. Returns `True` on success. |
| `get_navigation()` | Get current navigation data (non-blocking). |
| `wait_and_get_fresh_navigation()` | Block until new navigation data arrives. Releases GIL. |
| `name()` | Get detected HAT name (e.g. `'L1/L5 GNSS TIME HAT'`). |
| `soft_reset_hot_start()` | Software reset — preserves almanac/ephemeris for fast TTFF. |
| `hard_reset_cold_start()` | Hardware reset — full cold start. |
| `enable_timepulse()` | Enable 1PPS output on GPIO 5. |
| `disable_timepulse()` | Disable 1PPS output. |
| `timepulse()` | Block until next 1PPS interrupt. Call `enable_timepulse()` first. Releases GIL. |
| `start_forward_for_gpsd()` | Start NMEA forwarding to a virtual TTY for gpsd. |
| `stop_forward_for_gpsd()` | Stop NMEA forwarding. |
| `join_forward_for_gpsd()` | Block until forwarding thread exits. Releases GIL. |
| `get_gpsd_device_path()` | Get the virtual TTY path (e.g. `/dev/pts/3`). Returns `None` if not active. |
| `rtk_get_full_corrections()` | Get full RTCM3 correction frames from RTK base. Returns `list[bytes]`. |
| `rtk_get_tiny_corrections()` | Get compact RTCM3 correction frames from RTK base. Returns `list[bytes]`. |
| `rtk_get_rtcm3_frame(msg_id)` | Get a specific RTCM3 frame by message ID. Returns `bytes`. |
| `rtk_apply_corrections(frames)` | Send RTCM3 correction frames to RTK rover. Takes `list[bytes]`. |
| `get_time_mark()` | Get current time mark data (non-blocking). Returns `TimeMark` or `None`. |
| `wait_and_get_fresh_time_mark()` | Block until new time mark event arrives. Releases GIL. |
| `enable_time_mark_trigger()` | Enable EXTINT pin (GPIO 17) as output for software-triggered time marks. |
| `disable_time_mark_trigger()` | Disable EXTINT trigger. |
| `trigger_time_mark(edge)` | Trigger a time mark event. Optional `TimeMarkTriggerEdge` arg (default `TOGGLE`). |

Supports context manager:

```python
with gnsshat.GnssHat() as hat:
    hat.start(config)
    nav = hat.wait_and_get_fresh_navigation()
# __exit__ automatically stops GPSD forwarding and disables timepulse
```

## Module Functions and Constants

```python
gnsshat.version()              # Returns '1.0.0'
gnsshat.utc_time_iso8601(nav)  # Convert Navigation or PVT to ISO 8601 string

gnsshat.MAX_GEOFENCES   # 4
gnsshat.MAX_RF_BLOCKS    # 2
gnsshat.MAX_SATELLITES   # 64
```

## Navigation Data

`wait_and_get_fresh_navigation()` and `get_navigation()` return a `Navigation` object:

| Field | Type | Description |
|-------|------|-------------|
| `pvt` | `PositionVelocityTime` | Position, velocity, time, fix info |
| `dop` | `DilutionOverPrecision` | Dilution of precision values |
| `geofencing` | `Geofencing` | Geofencing config and status |
| `rf_blocks` | `list[RfBlock]` | RF band info (up to 2) |
| `satellites` | `list[SatelliteInfo]` | Tracked satellites (up to 64) |

All navigation objects have `__str__` and `__repr__` for easy printing.

### PositionVelocityTime

| Field | Type | Description |
|-------|------|-------------|
| `latitude` | `float` | Degrees |
| `longitude` | `float` | Degrees |
| `altitude` | `float` | Above WGS84 ellipsoid (m) |
| `altitude_msl` | `float` | Above mean sea level (m) |
| `speed_over_ground` | `float` | m/s |
| `speed_accuracy` | `float` | m/s |
| `heading` | `float` | Degrees |
| `heading_accuracy` | `float` | Degrees |
| `horizontal_accuracy` | `float` | Meters |
| `vertical_accuracy` | `float` | Meters |
| `visible_satellites` | `int` | Number of satellites used |
| `fix_quality` | `int` | `FixQuality` enum value |
| `fix_status` | `int` | `FixStatus` enum value |
| `fix_type` | `int` | `FixType` enum value |
| `utc_time` | `UtcTime` | `.hours`, `.minutes`, `.seconds`, `.valid`, `.accuracy` (ns) |
| `date` | `Date` | `.day`, `.month`, `.year`, `.valid` |

### DilutionOverPrecision

| Field | Type | Description |
|-------|------|-------------|
| `geometric` | `float` | GDOP — overall geometric quality |
| `position` | `float` | PDOP — 3D position |
| `time` | `float` | TDOP — time |
| `vertical` | `float` | VDOP — vertical position |
| `horizontal` | `float` | HDOP — horizontal position |
| `northing` | `float` | NDOP — northing |
| `easting` | `float` | EDOP — easting |

### RfBlock

| Field | Type | Description |
|-------|------|-------------|
| `id` | `int` | `RfBand` enum value |
| `jamming_state` | `int` | `JammingState` enum value |
| `antenna_status` | `int` | `AntennaStatus` enum value |
| `antenna_power` | `int` | `AntennaPower` enum value |
| `noise_per_ms` | `int` | Noise level |
| `agc_monitor` | `float` | AGC monitor percentage |
| `cw_interference_suppression_level` | `float` | CW suppression level |

### SatelliteInfo

| Field | Type | Description |
|-------|------|-------------|
| `gnss_id` | `int` | `GnssId` enum value |
| `sv_id` | `int` | Satellite vehicle ID |
| `cno` | `int` | Carrier-to-noise ratio (dBHz) |
| `elevation` | `int` | Degrees (-90 to 90) |
| `azimuth` | `int` | Degrees (0 to 360) |
| `quality` | `int` | `SvQuality` enum value |
| `used_in_fix` | `bool` | Whether satellite is used in fix |
| `healthy` | `bool` | Satellite health flag |

### Geofencing

| Field | Type | Description |
|-------|------|-------------|
| `cfg` | `GeofencingCfg` | `.confidence_level`, `.geofences` (list of `Geofence`) |
| `nav` | `GeofencingNav` | `.status`, `.number_of_geofences`, `.combined_state`, `.geofences` (list of status ints) |

### TimeMark

Returned by `get_time_mark()` and `wait_and_get_fresh_time_mark()`.

| Field | Type | Description |
|-------|------|-------------|
| `channel` | `int` | Time mark channel |
| `mode` | `int` | `TimeMarkMode` enum value |
| `run` | `int` | `TimeMarkRun` enum value |
| `time_base` | `int` | `TimeMarkTimeBase` enum value |
| `new_rising_edge` | `bool` | New rising edge detected |
| `new_falling_edge` | `bool` | New falling edge detected |
| `utc_available` | `bool` | UTC available |
| `time_valid` | `bool` | Time valid |
| `count` | `int` | Time mark event count |
| `week_number_rising` | `int` | GPS week number of rising edge |
| `week_number_falling` | `int` | GPS week number of falling edge |
| `tow_rising_ms` | `int` | TOW of rising edge (ms) |
| `tow_sub_rising_ns` | `int` | Sub-millisecond TOW of rising edge (ns) |
| `tow_falling_ms` | `int` | TOW of falling edge (ms) |
| `tow_sub_falling_ns` | `int` | Sub-millisecond TOW of falling edge (ns) |
| `accuracy_estimate_ns` | `int` | Accuracy estimate (ns) |

## Enums

All enums are `IntEnum` types accessed as `gnsshat.<EnumName>.<MEMBER>`.

| Enum | Members |
|------|---------|
| `DynamicModel` | `PORTABLE`, `STATIONARY`, `PEDESTRIAN`, `AUTOMOTIVE`, `SEA`, `AIRBORNE_1G`, `AIRBORNE_2G`, `AIRBORNE_4G`, `WRIST`, `BIKE`, `MOWER`, `ESCOOTER` |
| `TimepulsePolarity` | `FALLING_EDGE`, `RISING_EDGE` |
| `PioPinPolarity` | `LOW_MEANS_INSIDE`, `LOW_MEANS_OUTSIDE` |
| `FixQuality` | `INVALID`, `GPS_FIX_2D_3D`, `DGNSS`, `PPS_FIX`, `FIXED_RTK`, `FLOAT_RTK`, `DEAD_RECKONING` |
| `FixStatus` | `VOID`, `ACTIVE` |
| `FixType` | `NO_FIX`, `DEAD_RECKONING_ONLY`, `FIX_2D`, `FIX_3D`, `GNSS_WITH_DEAD_RECKONING`, `TIME_ONLY_FIX` |
| `GeofenceStatus` | `UNKNOWN`, `INSIDE`, `OUTSIDE` |
| `GeofencingStatus` | `NOT_AVAILABLE`, `ACTIVE` |
| `RfBand` | `L1`, `L2_OR_L5` |
| `JammingState` | `UNKNOWN`, `OK_NO_SIGNIFICANT_JAMMING`, `WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK`, `CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX` |
| `AntennaStatus` | `INIT`, `DONT_KNOW`, `OK`, `SHORT`, `OPEN` |
| `AntennaPower` | `OFF`, `ON`, `DONT_KNOW` |
| `RtkMode` | `BASE`, `ROVER` |
| `BaseMode` | `SURVEY_IN`, `FIXED_POSITION` |
| `FixedPositionType` | `ECEF`, `LLA` |
| `GnssId` | `GPS`, `SBAS`, `GALILEO`, `BEIDOU`, `IMES`, `QZSS`, `GLONASS` |
| `SvQuality` | `NO_SIGNAL`, `SEARCHING`, `SIGNAL_ACQUIRED`, `SIGNAL_DETECTED_BUT_UNUSABLE`, `CODE_LOCKED_AND_TIME_SYNCHRONIZED`, `CODE_AND_CARRIER_LOCKED_1`, `CODE_AND_CARRIER_LOCKED_2`, `CODE_AND_CARRIER_LOCKED_3` |
| `TimeMarkMode` | `SINGLE`, `RUNNING` |
| `TimeMarkRun` | `ARMED`, `STOPPED` |
| `TimeMarkTimeBase` | `RECEIVER`, `GNSS`, `UTC` |
| `TimeMarkTriggerEdge` | `RISING`, `FALLING`, `TOGGLE` |
