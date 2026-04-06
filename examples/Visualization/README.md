# GNSS Real-Time Visualization

A web application for real-time GNSS data visualization from the u-blox NEO-M9N module (Jimmy Paputto GNSS HAT).  
A Flask + Socket.IO server streams navigation data to the browser — runs on Raspberry Pi and is accessible from any device on the local network.

![Stack](res/NazwaStos.svg)

---

## Operating Modes

The application supports **three data source modes**:

| Mode | Description | Requirements |
|------|-------------|--------------|
| `native` | Direct module communication via the GnssHat library (SPI) | Installed `jimmypaputto` (GnssHat) library |
| `external_tty` | NMEA sentence reading from a serial port (`/dev/jimmypaputto/gnss`) | `pyserial`, `pynmea2`, connected UART port |
| `ros2` | ROS 2 topic subscription to `/gnss/navigation` | ROS 2 environment, `jp_gnss_hat` package |

---

## Getting Started

### Install Dependencies

```bash
cd examples/Visualization
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Native Mode (default)

Direct communication with the GNSS module through the GnssHat library. Requires root privileges (SPI).

```bash
python3 app.py
# or explicitly:
python3 app.py native
```

### External TTY Mode

Reads raw NMEA sentences from a serial port. Useful when the module is exposed as a TTY device.

```bash
python3 app.py external_tty
```

Default port: `/dev/jimmypaputto/gnss` @ 9600 baud (configurable via `SERIAL_PORT` / `BAUD_RATE` constants in `app.py`).

### ROS 2 Mode

Subscribes to the navigation topic from the `jp_gnss_hat` ROS 2 node.

```bash
# Without node name — topic: /gnss/navigation
python3 app.py ros2

# With node name (e.g. rover) — topic: /gnss/rover/navigation
python3 app.py ros2 rover

# With node name (e.g. base) — topic: /gnss/base/navigation
python3 app.py ros2 base
```

> **Note:** Before launching, source the ROS 2 workspace:
> ```bash
> source /opt/ros/<distro>/setup.bash
> source ~/ros-ws/install/setup.bash
> python3 app.py ros2 [node_name]
> ```

#### Topic and Service Names in ROS 2 Mode

The optional `node_name` argument affects the topic and configuration service prefixes:

| Command | Navigation Topic | get_config Service | set_config Service |
|---------|-----------------|-------------------|-------------------|
| `app.py ros2` | `/gnss/navigation` | `/gnss/get_config` | `/gnss/set_config` |
| `app.py ros2 rover` | `/gnss/rover/navigation` | `/gnss/rover/get_config` | `/gnss/rover/set_config` |
| `app.py ros2 base` | `/gnss/base/navigation` | `/gnss/base/get_config` | `/gnss/base/set_config` |

This allows simultaneous monitoring of multiple nodes (e.g. base and rover in an RTK setup) — each in its own application instance.

---

## Accessing the Interface

After startup, the server listens on port **5000**:

- From the Raspberry Pi: **http://localhost:5000**
- From the local network: **http://\<raspberry-pi-ip\>:5000**

---

## Interface Features

### Maps (tabs)

| Tab | Description |
|-----|-------------|
| **Relative Map** | Canvas map — current position relative to a reference point (offset in meters N/E). Adjustable scale range. |
| **Terrain Map** | OpenStreetMap (Leaflet) — real-time position on a terrain map. |
| **Sky View** | Polar sky plot with satellites (elevation/azimuth), color-coded by constellation (GPS, Galileo, GLONASS, BeiDou, SBAS, QZSS). Includes a table of tracked satellites. Available in `native` and `ros2` modes. |

### Navigation Data Panel

- **Time & Date** — UTC time and date from the receiver
- **Fix Information** — fix quality (2D/3D/RTK Fixed/RTK Float/DGPS), status, type, satellite count
- **Position** — latitude/longitude, MSL and WGS84 altitude
- **Velocity** — ground speed, heading
- **Accuracy** — horizontal, vertical, speed, and heading accuracy *(native/ros2)*
- **Dilution of Precision** — GDOP, PDOP, HDOP, VDOP, TDOP, NDOP, EDOP *(native/ros2)*
- **Geofencing** — status and number of active geofences *(native/ros2)*
- **RF / Antenna** — band (L1/L2), jamming state, antenna status, AGC, noise *(native/ros2)*

### Module Configuration (native / ros2)

The **Configuration** tab allows reading and changing the GNSS module configuration live:

- **Measurement Rate** — measurement frequency (1–25 Hz)
- **Dynamic Model** — dynamic model (Stationary, Pedestrian, Automotive, Sea, Airborne, Bike, E-Scooter, …)
- **Timepulse Pin** — PPS pulse configuration (frequency, pulse width, polarity, no-fix mode)
- **Geofencing** — define geofence zones (lat/lon/radius, confidence level)
- **RTK** — RTK mode (Base/Rover), base configuration (Survey-In or Fixed Position ECEF/LLA)
- **ROS 2** — ROS-specific options: publish standard topics, use NTRIP RTCM *(ros2 only)*

Buttons:
- **📥 Load Config** — reads the current configuration from the module/node
- **🚀 Send Config** — sends a new configuration (with a progress bar)

### Other

- **Reset Origin** — resets the reference point to the current position (button in the status bar)
- Real-time updates via **WebSocket** (Socket.IO)

---

## File Structure

```
Visualization/
├── app.py                 # Flask server + data source logic
├── requirements.txt       # Python dependencies
├── res/
│   └── NazwaStos.svg      # Logo
├── static/
│   ├── css/
│   │   └── style.css      # UI styles
│   └── js/
│       └── map.js         # Front-end logic (maps, charts, Socket.IO, configuration)
└── templates/
    └── index.html          # HTML template (Jinja2, conditional rendering by mode)
```

---

## System Requirements

- Python 3.10+
- Raspberry Pi with GNSS HAT module (native mode) or any machine with serial port access / ROS 2
- Browser with WebSocket support (any modern browser)
