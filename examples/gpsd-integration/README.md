# JP GNSS to GPSD Bridge

Two ways to forward GNSS data to gpsd:

1. **`jpgnss2gpsd-bridge`** — standalone systemd daemon, runs in background
2. **`GpsdInteractive`** — example for embed NMEA forwarding in your own application

Both create `/dev/jimmypaputto/gnss` virtual serial port that gpsd reads.

L1 GNSS HAT and L1/L5 RTK HAT use SPI — gpsd can't read SPI directly. The bridge reads NMEA from the HAT and exposes it as a virtual serial device that gpsd understands.

The L1/L5 TIME HAT uses UART (`/dev/ttyAMA0`) so gpsd can talk to it directly. The bridge still works with TIME HAT and simplifies the setup.

> **USB shortcut (L1 HAT & RTK HAT only):** The L1 GNSS HAT and L1/L5 RTK HAT have an exposed USB port connected directly to the u-blox module. Plug a USB cable from the HAT to your Raspberry Pi (or any host) and the module appears as `/dev/ttyACM0` (CDC-ACM serial device). gpsd can read this device directly - no bridge daemon and no library needed. See **Option C** below.

## Embedded forwarding (GpsdInteractive)

`GpsdInteractive` is an example — adapt the same mechanism in your own code. Your application can forward NMEA to gpsd while doing its own work at the same time. The forwarding runs in a background thread — no collisions with navigation reads.

```cpp
auto* ubxHat = IGnssHat::create();
ubxHat->start(config);

// Start NMEA forwarding in background
ubxHat->startForwardForGpsd();
printf("gpsd device: %s\n", ubxHat->getGpsdDevicePath().c_str());

// Your app keeps running — navigation and gpsd work simultaneously
while (running)
{
    auto nav = ubxHat->waitAndGetFreshNavigation();
    // use nav...
}

ubxHat->stopForwardForGpsd();
```

Build and run:

```bash
cd examples/GpsdIntegration
mkdir -p build && cd build
cmake .. && make
sudo ./GpsdInteractive
```

Then in another terminal:

```bash
cgps
```

Key API calls:
- `startForwardForGpsd()` — creates virtual serial port, starts NMEA thread
- `getGpsdDevicePath()` — returns path to virtual device (`/dev/jimmypaputto/gnss`)
- `stopForwardForGpsd()` — stops forwarding, removes virtual device
- `joinForwardForGpsd()` — blocks until forwarding is stopped

## Daemon (jpgnss2gpsd-bridge)

## How it works

```
GNSS HAT (SPI/UART) → jpgnss2gpsd-bridge → /dev/jimmypaputto/gnss → gpsd
```

The daemon:
- Auto-detects HAT type via `/proc/device-tree/hat/product`
- Starts the GNSS module with 1 Hz measurement rate
- Creates `/dev/jimmypaputto/gnss` virtual serial port (9600 baud, 8N1)
- Forwards NMEA sentences (GGA, RMC, GSA) at 1 Hz
- Runs as systemd service, restarts on failure

## Build

```bash
cd examples/GpsdIntegration
mkdir -p build && cd build
cmake ..
make
```

## Install

```bash
cd examples/GpsdIntegration
sudo ./scripts/install_daemon.sh
```

Or manually:

```bash
cd examples/GpsdIntegration/build
sudo make install
sudo systemctl daemon-reload
sudo systemctl enable jpgnss2gpsd-bridge
sudo systemctl start jpgnss2gpsd-bridge
```

Installs to `/usr/local/bin/jimmypaputto/jpgnss2gpsd-bridge`.

Verify:

```bash
ls /dev/jimmypaputto/gnss
cat /dev/jimmypaputto/gnss
sudo systemctl status jpgnss2gpsd-bridge
```

## Optional: PPS support

All three HATs output a PPS (Pulse Per Second) signal on **GPIO 5**. If you want gpsd to use PPS for precise timing, enable the kernel PPS driver first:

Add to `/boot/firmware/config.txt`:

```
dtoverlay=pps-gpio,gpiopin=5
```

Reboot, then verify:

```bash
sudo apt install pps-tools
sudo dmesg | grep pps
ls /dev/pps0
sudo ppstest /dev/pps0
```

> **Note:** Kernel PPS (`/dev/pps0`) and the library's `IGnssHat::timepulse()` callback both use GPIO 5 — they cannot run at the same time.

If you skip this step, gpsd works fine without PPS — you just won't get sub-microsecond timing. For a full PPS + chrony time server setup, see [../TimeServer/README.md](../TimeServer/README.md).

## Configure gpsd

### Option A: With bridge (L1 HAT, RTK HAT or TIME HAT)

```bash
sudo apt install gpsd gpsd-clients
```

Auto-configure (and skip rest to **Verify**):

```bash
sudo ./scripts/configure_gpsd.sh              # without PPS
sudo ./scripts/configure_gpsd.sh --with-pps   # with PPS (adds /dev/pps0 to DEVICES)
```

Or manually — without PPS:

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/jimmypaputto/gnss"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

With PPS:

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/jimmypaputto/gnss /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

Then make gpsd depend on the bridge:

```bash
sudo mkdir -p /etc/systemd/system/gpsd.service.d

sudo tee /etc/systemd/system/gpsd.service.d/override.conf > /dev/null <<EOF
[Unit]
Requires=jpgnss2gpsd-bridge.service
After=jpgnss2gpsd-bridge.service
ConditionPathExists=/dev/jimmypaputto/gnss

[Service]
Restart=always
RestartSec=5
EOF

sudo systemctl daemon-reload
sudo systemctl enable gpsd
sudo systemctl start gpsd
```

### Option B: Direct UART (TIME HAT only, no bridge needed)

Disable serial console and enable hardware UART:

```bash
sudo raspi-config nonint do_serial_hw 0
sudo raspi-config nonint do_serial_cons 1
sudo reboot
```

After reboot, set baud rate:

```bash
sudo stty -F /dev/ttyAMA0 115200
```

Without PPS:

```bash
sudo apt install gpsd gpsd-clients

sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/ttyAMA0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF

sudo systemctl daemon-reload
sudo systemctl enable gpsd
sudo systemctl start gpsd
```

With PPS:

```bash
sudo apt install gpsd gpsd-clients

sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/ttyAMA0 /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF

sudo systemctl daemon-reload
sudo systemctl enable gpsd
sudo systemctl start gpsd
```

No systemd override needed - gpsd reads UART directly, skip bridge install entirely.

### Option C: Direct USB (L1 HAT & RTK HAT only, no bridge needed)

The L1 GNSS HAT (NEO-M9N) and L1/L5 RTK HAT (NEO-F9P) have a USB port connected directly to the u-blox module. Connect a USB cable from the HAT to the Raspberry Pi (or any other host). The module enumerates as a CDC-ACM serial device — typically `/dev/ttyACM0`.

No bridge daemon, no library installation, and no SPI configuration required. The u-blox module outputs NMEA on USB by default.

Verify the device appeared:

```bash
ls /dev/ttyACM*
# expected: /dev/ttyACM0
```

Configure gpsd:

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/ttyACM0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

With PPS:

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/ttyACM0 /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

Start gpsd:

```bash
sudo systemctl daemon-reload
sudo systemctl enable gpsd
sudo systemctl start gpsd
```

No systemd override needed - gpsd reads USB serial directly, skip bridge install entirely.

> **Note:** When using USB, the u-blox module communicates with gpsd independently from the library. You can use both simultaneously - the library talks over SPI while gpsd reads USB - but keep in mind that configuration changes made by the library (measurement rate, dynamic model, etc.) will affect the NMEA output on USB as well.

## Verify

```bash
cgps
gpsmon
```

## Service control

```bash
sudo systemctl start jpgnss2gpsd-bridge
sudo systemctl stop jpgnss2gpsd-bridge
sudo systemctl restart jpgnss2gpsd-bridge
sudo systemctl status jpgnss2gpsd-bridge
sudo journalctl -u jpgnss2gpsd-bridge -f
```

## Configuration scripts

Helper scripts in `scripts/`:

| Script | Description |
|--------|-------------|
| `install_daemon.sh` | install bridge as systemd service |
| `uninstall_daemon.sh` | Remove bridge service and binary |
| `configure_gpsd.sh` | Auto-configure gpsd (add `--with-pps` for PPS) |

## Time server

Once gpsd is running, you can set up a PPS-disciplined time server with sub-microsecond accuracy. See [../TimeServer/README.md](../TimeServer/README.md) for the full setup guide.

## Uninstall

### Bridge daemon

```bash
sudo ./scripts/uninstall_daemon.sh
```

Or manually:

```bash
sudo systemctl stop jpgnss2gpsd-bridge
sudo systemctl disable jpgnss2gpsd-bridge
sudo rm /etc/systemd/system/jpgnss2gpsd-bridge.service
sudo rm /usr/local/bin/jimmypaputto/jpgnss2gpsd-bridge
sudo systemctl daemon-reload
```

### gpsd

```bash
sudo systemctl stop gpsd
sudo systemctl disable gpsd
sudo rm -rf /etc/systemd/system/gpsd.service.d
sudo rm /etc/default/gpsd
sudo apt remove --purge gpsd gpsd-clients
sudo systemctl daemon-reload
```

### PPS

Remove PPS overlay from `/boot/firmware/config.txt`:

```bash
sudo sed -i '/dtoverlay=pps-gpio,gpiopin=5/d' /boot/firmware/config.txt
sudo apt remove --purge pps-tools
sudo reboot
```

## Troubleshooting

**Bridge won't start:**
```bash
sudo journalctl -u jpgnss2gpsd-bridge -f
# Common cause: HAT not detected — check /proc/device-tree/hat/product
```

**No `/dev/jimmypaputto/gnss`:**
```bash
sudo systemctl status jpgnss2gpsd-bridge
# Restart if needed
sudo systemctl restart jpgnss2gpsd-bridge
```

**gpsd shows no data:**
```bash
# Check bridge output first
cat /dev/jimmypaputto/gnss
# If NMEA flows here but cgps is empty — check /etc/default/gpsd device path
```

**Debug gpsd in foreground:**
```bash
# Stop running instance first, then start manually with debug output
sudo systemctl stop gpsd
sudo gpsd -N -D5 /dev/jimmypaputto/gnss
```

**Manual gpsd test (foreground with debug):**
```bash
sudo systemctl stop gpsd
sudo gpsd -N -D5 /dev/jimmypaputto/gnss
```
