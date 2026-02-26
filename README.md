# JP_GNSS_HAT

The `JP_GNSS_HAT` library integrates our gnss hats with the Raspberry Pi via SPI. The aim of this software is to provide an easy way to configure ulox GNSS module and read all necessary data, without need of study whole ubx protocol specification and implementation insights of ublox. You can buy our hats here: https://jimmypaputto.com, if you have your custom hardware it might be beneficial for you too as most of the code is handling the UBX related stuff. This lib can be easily integrated with any environment as it provides the C headers, python module and all core threads/jobs are independent of the destination system architecture. 
## Table of Contents

- [Introduction](#introduction)
- [Installation](#installation)
- [Usage](#usage)
- [Structures](#structures)
- [Threads](#threads)
- [Hat Specs](#hatspecs)
- [License](#license)

## Introduction

The `JP_GNSS_HAT` library is a complete driver for u-blox GNSS modules mounted on Jimmy Paputto GNSS HATs for the Raspberry Pi. It communicates with the receiver via SPI (L1 HAT) or UART (L1/L5 TIME HAT), handles the full UBX binary protocol and provides a clean, high-level API in **C++**, **C** and **Python**.

Key features:
- **Plug-and-play** — auto-detects the HAT variant (NEO-M9N / NEO-F10T) and configures the communication interface accordingly
- **Configurable measurement rate** (1–25 Hz), dynamic models, timepulse output and geofencing (up to 4 zones)
- **Navigation data** — latitude, longitude, altitude, speed, heading, UTC time, DOP values, satellite info, RF/jamming diagnostics
- **RTK support** — base-station survey-in / fixed-position modes and rover correction injection
- **GPSD integration** — built-in NMEA forwarding to a virtual serial port (`/tmp/ttyJPGNSS`) for the gpsd daemon
- **Multi-threaded, event-driven architecture** — interrupt/epoll-based data flow with no busy-wait loops
- **Thread-safe** — all navigation accessors are mutex-protected; multiple consumer threads are supported out of the box

## Installation

Before you start, make sure you have those installed:

```
sudo apt-get install cmake git libgpiod-dev
```
To install the library, follow these steps:

1. Clone the repository:
    ```sh
    git clone https://github.com/jimmypaputto/GnssHat.git
    cd GnssHat
    ```

2. Build and install the library:
    ```sh
    mkdir -p build
    cd build
    cmake ..
    make
    sudo make install
    sudo ldconfig
    ```

3. Optional, build samples:
    ```sh
    ./scripts/build_all_examples.sh
    ```

## Usage

To use the `GnssHat` library in your project, include the header `GnssHat.hpp`, link library and use the `IGnssHat` interface. For advices in usage check examples dir. For C language use `GnssHat.h`.

### Usage Example (TLDR)

Here's a basic example to get you started quickly:

#### C++ Example

```cpp
#include <chrono>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>

void print(const Navigation& navigation)
{
    printf("Latitude: %f\n", navigation.pvt.latitude);
    printf("Longitude: %f\n", navigation.pvt.longitude);
    printf("Altitude: %f\n", navigation.pvt.altitude);
}

auto main() -> int
{
    auto* ubxHat = JimmyPaputto::IGnssHat::create();

    JimmyPaputto::GnssConfig config;
    config.measurementRate_Hz = 1;
    config.dynamicModel = JimmyPaputto::EDynamicModel::Portable;
    config.timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.1f },
        .pulseWhenNoFix = std::nullopt,
        .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    config.geofencing = std::nullopt;

    const bool isStartupDone = ubxHat->start(config);
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }

    while (true)
    {
        const auto navigation = ubxHat->navigation();
        print(navigation);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

#### Python Example

```python
from jimmypaputto import gnsshat

def print_position(navigation):
    print(f"Latitude: {navigation.pvt.latitude}")
    print(f"Longitude: {navigation.pvt.longitude}")
    print(f"Altitude: {navigation.pvt.altitude}")

def main():
    hat = gnsshat.GnssHat()
    
    config = {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.PORTABLE,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'pulse_when_no_fix': None,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None
    }
    
    hat.start(config)
    
    while True:
        navigation = hat.wait_and_get_fresh_navigation()
        print_position(navigation)

if __name__ == "__main__":
    main()
```

**That's it!** For more detailed configuration options, structures description, threading patterns, and advanced features, see the sections below and check examples dir.

### Structures

The library provides several key structures. First one is 'GnssConfig' which stores configuration data for ublox, for example measurementRate \[Hz\]. The second one is 'Navigation' which stores all data from GNSS, like lat, lon, UTC, etc. Detailed description below. 

- `GnssConfig`: Configuration for the GNSS module. It includes settings:
  - `measurementRate_Hz` (uint16_t): GNSS measurement rate in Hz (1-25 Hz). Determines how frequently the GNSS module takes measurements.
  - `dynamicModel` (EDynamicModel): Motion model for the receiver. Options include:
    - `Portable`: For general handheld devices
    - `Stationary`: For fixed installations
    - `Pedestrian`: For walking applications
    - `Automotive`: For car navigation
    - `Sea`: For marine applications
    - `Airborne1G`, `Airborne2G`, `Airborne4G`: For different aircraft applications
    - `Wrist`: For wearable devices
    - `Bike`: For bicycle navigation
    - `Mower`: For robotic lawn mowers
    - `Escooter`: For electric scooter navigation
  - `timepulsePinConfig` (TimepulsePinConfig): Time pulse signal configuration:
    - `active` (bool): Enable/disable time pulse output
    - `fixedPulse` (Pulse): Pulse settings when GNSS has a fix
      - `frequency` (uint32_t): Pulse frequency in Hz
      - `pulseWidth` (float): Pulse width as fraction (0.0-0.99, typical value is 0.1)
    - `pulseWhenNoFix` (optional<Pulse>): Pulse settings when no GNSS fix available
    - `polarity` (ETimepulsePinPolarity): Pulse edge polarity (rising/falling edge at top of second)
  - `geofencing` (optional<Geofencing>): Geofencing configuration (optional):
    - `geofences` (vector<Geofence>): List of geofences (max 4):
      - `lat` (float): Latitude in degrees (-90.0 to 90.0)
      - `lon` (float): Longitude in degrees (-180.0 to 180.0)
      - `radius` (float): Radius in meters (must be positive)
    - `confidenceLevel` (uint8_t): Confidence level for geofencing (0-5). Determines the statistical confidence required for geofence state detection:
      - `0`: No confidence required (immediate detection)
      - `1`: 68% confidence level (1 sigma)
      - `2`: 95% confidence level (2 sigma)
      - `3`: 99.7% confidence level (3 sigma)
      - `4`: 99.99% confidence level (4 sigma)
      - `5`: 99.9999% confidence level (5 sigma)

- `Navigation`: Contains GNSS data:
  - `DilutionOverPrecision` (DOP): Dilution of Precision values indicating GNSS accuracy:
    - `geometric` (float): Geometric DOP - overall satellite geometry quality
    - `position` (float): Position DOP - 3D position accuracy
    - `time` (float): Time DOP - time accuracy
    - `vertical` (float): Vertical DOP - altitude accuracy
    - `horizontal` (float): Horizontal DOP - horizontal position accuracy
    - `northing` (float): Northing DOP - north-south position accuracy
    - `easting` (float): Easting DOP - east-west position accuracy
  - `PositionVelocityTime` (PVT): Position, velocity, and time data:
    - `fixQuality` (EFixQuality): GNSS fix quality (determined by correction method, not accuracy, use `horizontalAccuracy` and `verticalAccuracy` fields for real-time precision estimates):
      - `Invalid`: No valid fix
      - `GpsFix2D3D`: Standard GPS 2D/3D fix (typical 3-10m, but can vary). Uses satellite signals only
      - `DGNSS`: Differential GPS fix (typical 1-5m). Uses reference stations for real-time error correction (u-blox automaticly connects with SBAS by geostationary sattelites)
      - `PpsFix`: Precise Positioning Service fix (typical 0.1-1m). Military/commercial high-precision service
      - `FixedRTK`: RTK fixed solution (typical 1-10cm). Uses carrier phase measurements with ambiguity resolution
      - `FloatRtk`: RTK float solution (typical 10-100cm). Uses carrier phase but without full ambiguity resolution
      - `DeadReckoning`: Dead reckoning only. Uses inertial sensors when GNSS unavailable
    - `fixStatus` (EFixStatus): Fix status (`Void` or `Active`)
    - `fixType` (EFixType): Type of fix:
      - `NoFix`: No position fix
      - `DeadReckoningOnly`: Dead reckoning only
      - `Fix2D`: 2D position fix
      - `Fix3D`: 3D position fix
      - `GnssWithDeadReckoning`: GNSS + dead reckoning combined
      - `TimeOnlyFix`: Time-only fix
    - `utc` (UTC): UTC time information:
      - `hh` (uint8_t): Hours (0-23)
      - `mm` (uint8_t): Minutes (0-59)
      - `ss` (uint8_t): Seconds (0-59)
      - `valid` (bool): Time validity flag
      - `accuracy` (int32_t): Time accuracy in nanoseconds
    - `date` (Date): Date information:
      - `day` (uint8_t): Day of month (1-31)
      - `month` (uint8_t): Month (1-12)
      - `year` (uint16_t): Year
      - `valid` (bool): Date validity flag
    - `altitude` (float): Altitude above ellipsoid in meters
    - `altitudeMSL` (float): Altitude above mean sea level in meters
    - `latitude` (double): Latitude in degrees (-90.0 to 90.0)
    - `longitude` (double): Longitude in degrees (-180.0 to 180.0)
    - `speedOverGround` (float): Ground speed in m/s
    - `speedAccuracy` (float): Speed accuracy estimate in m/s
    - `heading` (float): Heading of motion in degrees (0-360)
    - `headingAccuracy` (float): Heading accuracy estimate in degrees
    - `visibleSatellites` (uint8_t): Number of visible satellites
    - `horizontalAccuracy` (float): Horizontal position accuracy in meters
    - `verticalAccuracy` (float): Vertical position accuracy in meters
  - `Geofencing`: Geofencing data with configuration and navigation:
    - `cfg` (Geofencing::Cfg): Geofencing configuration (from UBX-CFG-GEOFENCE):
      - `pioPinNumber` (uint8_t): PIO pin number for geofencing output signal (pin 6 is hardcoded, drives LED and relay on HAT)
      - `pinPolarity` (EPioPinPolarity): PIO pin output polarity:
        - `LowMeansInside`: Low signal when inside geofence, high when outside
        - `LowMeansOutside`: Low signal when outside geofence, high when inside
      - `pioEnabled` (bool): Enable/disable PIO pin output for geofencing status
      - `confidenceLevel` (uint8_t): Confidence level for geofencing (0-5). Same as in GnssConfig:
        - `0`: No confidence required (immediate detection)
        - `1`: 68% confidence level (1 sigma)
        - `2`: 95% confidence level (2 sigma) 
        - `3`: 99.7% confidence level (3 sigma)
        - `4`: 99.99% confidence level (4 sigma)
        - `5`: 99.9999% confidence level (5 sigma)
      - `geofences` (vector<Geofence>): List of configured geofences (max 4), each with lat, lon, radius \[m\]
    - `nav` (Geofencing::Nav): Real-time geofencing navigation data:
      - `iTOW` (uint32_t): Time of week in milliseconds
      - `geofencingStatus` (EGeofencingStatus): Overall geofencing status (`NotAvailable` or `Active`)
      - `numberOfGeofences` (uint8_t): Number of active geofences
      - `combinedState` (EGeofenceStatus): Combined state of all geofences (`Unknown`, `Inside`, or `Outside`)
      - `geofencesStatus` (array<EGeofenceStatus, 4>): Individual status of each geofence
  - `RfBlock` (array, max 2): RF block data for interference monitoring:
    - `id` (EBand): RF band identifier (`L1` or `L2orL5`)
    - `jammingState` (EJammingState): Jamming detection status:
      - `Unknown`: Jamming state unknown
      - `Ok_NoSignifantJamming`: No significant jamming detected
      - `Warning_InferenceVisibleButFixOk`: Interference visible but fix OK
      - `Critical_InferenceVisibleAndNoFix`: Critical interference, no fix
    - `antennaStatus` (EAntennaStatus): Antenna status (`Init`, `DontKnow`, `Ok`, `Short`, `Open`)
    - `antennaPower` (EAntennaPower): Antenna power status (`Off`, `On`, `DontKnow`)
    - `postStatus` (uint32_t): POST (Power-On Self Test) status
    - `noisePerMS` (uint16_t): Noise level per millisecond
    - `agcMonitor` (float): AGC monitor value (percentage of max gain)
    - `cwInterferenceSuppressionLevel` (float): CW interference suppression level
    - `ofsI`, `magI`, `ofsQ`, `magQ`: I/Q imbalance values for RF calibration

### Threads

The library implements a sophisticated multi-threaded architecture designed for high-performance, real-time GNSS data processing. Understanding this architecture is crucial for optimal integration and performance tuning.

#### Thread Architecture Overview

The library's thread architecture varies depending on the HAT type:

##### L1 GNSS HAT (SPI-based)
The library operates with **5 distinct threads** that work together to provide seamless GNSS data flow:

1. **Main Application Thread** - Your application code
2. **Ublox Data Processing Thread** - Core GNSS data acquisition and parsing
3. **TxReady Interrupt Thread** - Hardware interrupt handling for data synchronization
4. **Timepulse Interrupt Thread** - Precise timing signal processing (optional)
5. **NMEA Forwarder Thread** - NMEA data streaming to virtual serial port for gpsd (optional)

##### L1/L5 GNSS TIME HAT (UART-based)
The library operates with **4 distinct threads** that work together to provide seamless GNSS data flow:

1. **Main Application Thread** - Your application code
2. **Ublox Data Processing Thread** - Core GNSS data acquisition and parsing (uses epoll for non-blocking I/O)
3. **Timepulse Interrupt Thread** - Precise timing signal processing (optional)
4. **NMEA Forwarder Thread** - optional, you can use our forwarder or connect directly to the ublox via uart without our software

**Key Difference**: L1/L5 GNSS TIME HAT does not require a TxReady interrupt thread as it uses UART with epoll-based polling instead of SPI with hardware synchronization.

#### Detailed Thread Analysis

##### 1. Ublox Data Processing Thread
**Purpose**: Core GNSS data acquisition and UBX protocol handling
**Lifecycle**: Created in `start()`, runs continuously until destruction
**Key Responsibilities**:
- Executes `Ublox::run()` in a continuous loop
- **L1 GNSS HAT**: Waits for TxReady hardware interrupt via `txReadyNotifier_.wait()`
- **L1/L5 GNSS TIME HAT**: Uses epoll-based non-blocking I/O on UART interface
- Reads raw GNSS data from SPI (L1 HAT) or UART (L1/L5 TIME HAT) interface
- Parses UBX protocol messages using `UbxParser`
- Updates internal `Gnss` navigation state
- Triggers `navigationNotifier_.notify()` after each successful data update

```cpp
// Thread creation in start() method
ubloxThread_ = std::thread([this](){
    while (ubloxThreadRunning_) {
        ublox_.run();  // Behavior depends on HAT type
    }
});
```

##### 2. TxReady Interrupt Thread
**Purpose**: Hardware synchronization for optimal SPI timing
**Hardware**: Monitors GPIO pin 17 (TxReady signal from Ublox module)
**Availability**: Only available on L1 GNSS HAT (SPI-based)
**Key Responsibilities**:
- Waits for rising edge interrupt from Ublox module indicating data ready
- Calculates adaptive timeout based on measurement rate: `timeout = 2s / measurementRate_Hz`
- Notifies Ublox thread when new data batch is available via `txReadyNotifier_.notify()`
- Prevents unnecessary SPI polling, reducing CPU usage and improving power efficiency

**Note**: L1/L5 GNSS TIME HAT does not use TxReady interrupts. Instead, it relies on UART with epoll-based polling for efficient data acquisition without blocking.

##### 3. Timepulse Interrupt Thread
**Purpose**: High-precision timing synchronization, can be used standalone without navigation data
**Hardware**: Monitors GPIO pin 5 (Timepulse signal from Ublox module)
**Availability**: Available on both HAT types
**Key Responsibilities**:
- Captures precise timing pulses (typically 1 PPS - pulse per second)
- Provides microsecond-accurate time synchronization via `timepulseNotifier_`
- Enables applications requiring precise timing (e.g., time servers, scientific measurements)

##### 4. Main Application Thread
**Your Code**: Where you call navigation methods
**Synchronization Options**:
- `waitAndGetFreshNavigation()`: **Blocking** - waits for next data update via `navigationNotifier_.wait()`
- `navigation()`: **Non-blocking** - returns immediately with current data
- `enableTimepulse()`: **Setup** - enables timepulse functionality (GPIO 5). **Note**: Call this only if not using gpsd/pps to avoid GPIO conflicts
- `disableTimepulse()`: **Cleanup** - disables timepulse and releases GPIO resources
- `timepulse()`: **Blocking** - waits for next timepulse signal (requires `enableTimepulse()` first)
- `startForwardForGpsd()`: **Setup** - creates virtual serial port (`/tmp/ttyJPGNSS`) and starts NMEA forwarding for gpsd integration
- `stopForwardForGpsd()`: **Cleanup** - stops NMEA forwarding and releases virtual serial port
- `getGpsdDevicePath()`: **Info** - returns the virtual serial port path for gpsd configuration

#### Thread Synchronization Flow

##### L1 GNSS HAT (SPI-based):
```
Hardware → Interrupt Threads → Data Thread → Application Thread
   ↓              ↓               ↓              ↓
Ublox      TxReady Thread    Ublox Thread   Your Code
Module  →  (GPIO 17)     →   Data Parser  → navigation()
   ↓              ↓               ↓              ↓
Timepulse → Timepulse Thread → Notification → timepulse()
(GPIO 5)       
```

##### L1/L5 GNSS TIME HAT (UART-based):
```
Hardware → Data Thread → Application Thread
   ↓           ↓              ↓
Ublox    Ublox Thread   Your Code
Module →  (epoll I/O) → navigation()
   ↓           ↓              ↓
Timepulse → Timepulse Thread → timepulse()
(GPIO 5)
```

#### Performance Characteristics

##### L1 GNSS HAT:
- No polling loops - all threads are interrupt/event-driven
- SPI reads only when data is actually available (TxReady signaling)
- Adaptive timeouts based on configured measurement rate

##### L1/L5 GNSS TIME HAT:
- Uses efficient epoll mechanism for non-blocking I/O
- Simpler data flow with fewer synchronization points

#### Thread Safety
- All navigation data access is mutex-protected via `gnss_.lock()/unlock()`
- Atomic flags used for thread control (`ubloxThreadRunning_`, `threadRunning_`)
- Condition variables ensure proper thread synchronization

#### Best Practices for Integration

**General practices**:

1. **Real-time Applications**: Use `waitAndGetFreshNavigation()` for guaranteed fresh data
   ```cpp
   while (running) {
       auto nav = hat->waitAndGetFreshNavigation();  // Always fresh data
       processRealTimeData(nav);
   }
   ```

2. **UI/Monitoring Applications**: Use `navigation()` for non-blocking updates
   ```cpp
   // Update UI every 100ms without blocking
   auto nav = hat->navigation();  // Immediate return
   updateDisplay(nav);
   ```

3. **Timing-Critical Applications**: Enable timepulse first, then combine with navigation data
   ```cpp
   // Enable timepulse functionality (GPIO 5)
   if (hat->enableTimepulse()) {
       hat->timepulse();  // Blocks until next 1PPS pulse
       auto nav = hat->navigation();  // Get synchronized data
   }
   ```

4. **GPSD/PPS Integration**: DO NOT call `enableTimepulse()` when using gpsd/pps to avoid GPIO conflicts
   ```cpp
   // When using gpsd/pps, only use navigation methods:
   auto nav = hat->waitAndGetFreshNavigation();  // Safe with gpsd/pps
   // Do NOT call hat->enableTimepulse() or hat->timepulse()
   ```

6. **Multiple Consumers**: Multiple threads can safely call navigation methods simultaneously
   ```cpp
   // Thread 1: Real-time processing
   std::thread realtime([&]() {
       while (running) {
           processData(hat->waitAndGetFreshNavigation());
       }
   });
   
   // Thread 2: UI updates
   std::thread ui([&]() {
       while (running) {
           updateUI(hat->navigation());
           std::this_thread::sleep_for(100ms);
       }
   });
   ```

The thread architecture ensures optimal performance while maintaining ease of use, allowing applications to choose the most appropriate data access pattern for their specific requirements.

### Hat Specs

The JP_GNSS_HAT library automatically detects and supports two different HAT variants:

#### L1 GNSS HAT
- **Communication**: SPI interface
- **GNSS Module**: u-blox NEO-M9N (L1 band)
- **Thread Architecture**: 5 threads
- **Data Synchronization**: Hardware TxReady interrupt (GPIO 17)
- **Performance**: Interrupt-driven data acquisition with predictable timing
- **Use Cases**: General GNSS applications, robotics

#### L1/L5 GNSS TIME HAT  
- **Communication**: UART interface
- **GNSS Module**: u-blox NEO-F10T (L1/L5 dual-band)
- **Thread Architecture**: 4 threads
- **Data Synchronization**: epoll-based non-blocking I/O
- **Performance**: Lower latency, simplified architecture
- **Use Cases**: High-precision timing applications, high-frequency data acquisition, applications requiring dual-band GNSS

### License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.
