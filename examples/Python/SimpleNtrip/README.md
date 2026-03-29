# SimpleNtrip — RTK Base + Rover over Local Network

Quick-start RTK setup using two Raspberry Pi boards with GNSS RTK HATs.
One Pi acts as the **base station** (NTRIP caster), the other as the **rover** (NTRIP client).

```
  Pi 1 (Base)                Pi 2 (Rover)
┌────────────────┐        ┌────────────────┐
│ base.py        │        │ rover.py       │
│ GNSS RTK HAT   │        │ GNSS RTK HAT   │
│ (RTK Base)     │        │ (RTK Rover)    │
│      │         │        │      ▲         │
│      ▼         │        │      │         │
│ NTRIP Caster   │◄───────│ NTRIP Client   │
│ :2101          │  WiFi  │ (pygnssutils)  │
│ /MY_RTK_BASE   │  /LAN  │               │
└────────────────┘        └────────────────┘
```

## Prerequisites

Both Pi boards need the GNSS HAT library and Python bindings installed:

```bash
cd ~/GnssHat
scripts/install_deps.sh
mkdir -p build && cd build && cmake .. && make -j$(nproc) && sudo make install && sudo ldconfig
cd ~/GnssHat/python
mkdir -p build && cd build && cmake .. && make -j$(nproc) && sudo make install
```

## Setup (venv)

Create a virtual environment and install dependencies on each Pi:

```bash
cd ~/GnssHat/examples/Python/SimpleNtrip
python3 -m venv --system-site-packages venv
source venv/bin/activate
pip install -r requirements.txt
```

> `--system-site-packages` is required so the venv can access the
> `jimmypaputto` module installed system-wide by `sudo make install`.

## Configuration

### config_base.json (Base Pi)

| Field | Description |
|-------|-------------|
| `caster_ip` | Bind address for the caster (default: `"0.0.0.0"` = all interfaces) |
| `caster_port` | NTRIP port (default: `2101`) |
| `mountpoint` | Mountpoint name (default: `"MY_RTK_BASE"`) |
| `base_mode` | `"survey_in"` or `"fixed_position"` |
| `survey_in` | Survey-In parameters (observation time, accuracy) |
| `fixed_position` | Known position if using fixed mode (lat/lon/height) |

### config_rover.json (Rover Pi)

| Field | Description |
|-------|-------------|
| `caster_ip` | IP address of the base Pi (e.g. `"192.168.1.100"`) |
| `caster_port` | NTRIP port — must match the base config |
| `mountpoint` | Mountpoint name — must match the base config |
| `ntrip_version` | NTRIP protocol version (`"2.0"`) |

## Running

### 1. Start the base station (Pi 1)

```bash
cd ~/GnssHat/examples/Python/SimpleNtrip
source venv/bin/activate
python base.py
```

Wait for the log to show `TIME_ONLY_FIX` — this means the base has completed
Survey-In and is generating RTCM3 corrections.

### 2. Start the rover (Pi 2)

```bash
cd ~/GnssHat/examples/Python/SimpleNtrip
source venv/bin/activate
python rover.py
```

The rover will connect to the caster and start receiving corrections.
Watch the fix quality progress: `NO_FIX` → `GNSS_FIX` → `RTK_FLOAT` → `RTK_FIXED`.

### 3. Stop

Press `Ctrl+C` on either script for a clean shutdown.

## Files

| File | Description |
|------|-------------|
| `config_base.json` | Base station config (bind IP, port, mountpoint, base mode) |
| `config_rover.json` | Rover config (caster IP, port, mountpoint) |
| `requirements.txt` | Python dependencies (install via pip) |
| `base.py` | RTK Base station + NTRIP caster |
| `rover.py` | RTK Rover + NTRIP client |
| `ntrip_caster.py` | Lightweight NTRIP v2.0 caster (used by base.py) |

## Notes

- Both boards must have the **L1/L5 GNSS RTK HAT** (NEO-F9P).
- The caster has no authentication — it is designed for private LAN use.
- Default Survey-In requires 120 s observation and 50 m accuracy. For production
  use, lower `required_position_accuracy_m` (e.g. 2.0) and increase observation time.
- For a known fixed position, set `base_mode` to `"fixed_position"` and fill in
  the `fixed_position` fields in `config.json`.
