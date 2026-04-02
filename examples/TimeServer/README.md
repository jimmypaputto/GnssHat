# Raspberry Pi Time Server with PPS

This document show how to precise time synchronization using PPS (Pulse Per Second) signal from Jimmy Paputto GNSS HAT with chrony.

Works with all three HATs (L1 GNSS HAT, L1/L5 TIME HAT, L1/L5 RTK HAT). PPS output is on **GPIO 5**.

> **Important:** Enabling kernel PPS claims GPIO 5. You will **not** be able to use `IGnssHat::timepulse()` from the library while kernel PPS is active. Pick one or the other.

## Requirements

| Component | L1 GNSS HAT (NEO-M9N) | L1/L5 TIME HAT (NEO-F10T) | L1/L5 RTK HAT (NEO-F9P) |
|-----------|------------------------|----------------------------|--------------------------|
| jpgnss2gpsd-bridge | Required (or use USB) | Optional | Required (or use USB) |
| gpsd | **Required** | **Required** | **Required** |

L1 and RTK HATs use SPI - they need the bridge daemon (check GpsdIntegration in examples) to expose NMEA data to gpsd. Alternatively, the L1 and RTK HATs have an exposed USB port - plug a USB cable and the u-blox module appears as `/dev/ttyACM0`, which gpsd can read directly without the bridge (see step 3). The TIME HAT uses UART and can feed gpsd directly, but the bridge still works and simplifies the setup.

```bash
sudo apt-get update
sudo apt install pps-tools chrony gpsd gpsd-clients
```

## 1. Enable PPS in device tree

```bash
sudo <your_cmd_text_editor> /boot/firmware/config.txt
```

Add at the end:

```
dtoverlay=pps-gpio,gpiopin=5
```

Reboot:

```bash
sudo reboot
```

## 2. Load PPS kernel module

```bash
sudo modprobe pps-gpio
```

Make it permanent:

```bash
echo "pps-gpio" | sudo tee -a /etc/modules
```

Verify:

```bash
ls /dev/pps0
```

Test PPS signal (needs GNSS fix):

```bash
sudo ppstest /dev/pps0
```

Expected:

```
source 0 - assert 1234567890.123456789, sequence: 1
source 0 - assert 1234567891.123456789, sequence: 2
```

If no pulses - wait for GNSS fix or check antenna placement.

## 3. Install and start the bridge daemon [Required for L1 and RTK HATs unless using USB, Optional for TIME HAT]

Build and install from `examples/GpsdIntegration/`:

```bash
cd examples/GpsdIntegration
mkdir -p build && cd build
cmake ..
make
sudo make install
cd ..
sudo ./scripts/install_daemon.sh
```

Start:

```bash
sudo systemctl enable jpgnss2gpsd-bridge
sudo systemctl start jpgnss2gpsd-bridge
```

Verify virtual device exists:

```bash
ls /dev/jimmypaputto/gnss
```

### Alternative: USB direct (L1 HAT & RTK HAT only)

The L1 GNSS HAT and L1/L5 RTK HAT have a USB port wired directly to the u-blox module. Connect a USB cable from the HAT to the Raspberry Pi - the module appears as `/dev/ttyACM0` (CDC-ACM). This replaces the bridge daemon entirely.

```bash
ls /dev/ttyACM*
# expected: /dev/ttyACM0
```

If using USB, skip the bridge install above and use `/dev/ttyACM0` instead of `/dev/jimmypaputto/gnss` in the gpsd configuration (step 4). No systemd override is needed since there is no bridge dependency.

## 4. Configure gpsd

### Option A: Via bridge daemon (L1 HAT, L1/L5 RTK HAT or L1/L5 TIME HAT with bridge)

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/jimmypaputto/gnss /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

Make gpsd start after the bridge:

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
```

### Option B: Via direct UART access (TIME HAT only)

The TIME HAT (NEO-F10T) outputs NMEA natively on `/dev/ttyAMA0` at 38400 or 115200 baud. No bridge daemon needed.

Disable UART console first:

```bash
sudo raspi-config nonint do_serial_hw 0
sudo raspi-config nonint do_serial_cons 1
sudo reboot
```

After reboot, set baud rate and configure gpsd to use UART directly:

```bash
sudo stty -F /dev/ttyAMA0 115200
```

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/ttyAMA0 /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

No systemd override needed - gpsd talks to UART directly, skip step 3 entirely.

### Option C: Via direct USB access (L1 HAT & RTK HAT only)

If you connected the HAT via USB (see step 3 alternative), configure gpsd to use the USB serial device:

```bash
sudo tee /etc/default/gpsd > /dev/null <<EOF
DEVICES="/dev/ttyACM0 /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
```

No systemd override needed - gpsd reads USB serial directly.

### Start gpsd

```bash
sudo systemctl daemon-reload
sudo systemctl enable gpsd
sudo systemctl start gpsd
```

Quick check:

```bash
cgps
```

## 5. Configure chrony

Remove ntpd if present:

```bash
sudo systemctl stop ntp 2>/dev/null; sudo systemctl disable ntp 2>/dev/null; sudo apt remove ntp -y 2>/dev/null
```

Backup original config:

```bash
sudo cp /etc/chrony/chrony.conf /etc/chrony/chrony.conf.backup
```

Write config. To serve time to other machines on the LAN, uncomment the `allow` line and adjust the subnet to match your network (e.g. `allow 10.0.0.0/8` for `10.x.x.x`, `allow 192.168.1.0/24` for `192.168.1.x`). Without it chrony only syncs the local clock and rejects NTP queries from other hosts.

```bash
sudo tee /etc/chrony/chrony.conf > /dev/null <<EOF
# GNSS + PPS time server

# GPS time via shared memory from gpsd
refclock SHM 0 offset 0.0 delay 0.2 refid NMEA

# PPS — precise edge timing
refclock PPS /dev/pps0 refid PPS lock NMEA

# Fallback internet pools
pool 0.pool.ntp.org iburst
pool 1.pool.ntp.org iburst

# Allow LAN clients (adjust subnet as needed)
# allow 192.168.0.0/16

maxupdateskew 100.0
makestep 1000 3
rtcsync

log tracking measurements statistics
logdir /var/log/chrony
EOF
```

Create log directory and start:

```bash
sudo mkdir -p /var/log/chrony
sudo systemctl enable chrony
sudo systemctl restart chrony
```

## 6. Verify

Check time sources:

```bash
chronyc sources -v
```

Expected — both NMEA and PPS visible, PPS selected (`*`):

```
MS Name/IP address         Stratum Poll Reach LastRx Last sample               
===============================================================================
#- NMEA                          0   4   377    20    +52ms[  +52ms] +/-  100ms
#* PPS                           0   4   377    18   +451ns[+1019ns] +/-  181ns
^- ntp1.example.com              2   6    77    51   +870us[ +871us] +/-   37ms
^- ntp2.example.com              2   6    77    52   -186us[ -186us] +/-   16ms
```

`#*` on PPS means it's the selected source. NMEA shows `#-` (not combined) - that's normal, it serves as a coarse time reference for PPS lock. Internet servers (`^-`) are fallback only.

Check sync status:

```bash
chronyc tracking
```

PPS test:

```bash
sudo ppstest /dev/pps0
```

Service status:

```bash
sudo systemctl status jpgnss2gpsd-bridge
sudo systemctl status gpsd
sudo systemctl status chrony
```

## 7. Test from another machine

To verify the time server works over the network, point another Raspberry Pi (or any Linux box) at it.

On the **time server Pi**, make sure `allow` is uncommented in `/etc/chrony/chrony.conf` for your subnet. Then check its IP:

```bash
hostname -I
```

On the **client machine**, install chrony and point it at the server:

```bash
sudo apt install chrony
```

```bash
sudo tee /etc/chrony/chrony.conf > /dev/null <<EOF
server <TIME_SERVER_IP> iburst prefer
pool 0.pool.ntp.org iburst

makestep 1000 3
rtcsync
EOF
```

Replace `<TIME_SERVER_IP>` with the actual IP (e.g. `192.168.1.50`).

```bash
sudo systemctl restart chrony
```

### Measure accuracy

On the client, check offset to the time server:

```bash
chronyc sources -v
```

The `Last sample` column shows the offset. Values in low microseconds (`us`) mean it's working well over LAN.

For a one-shot measurement:

```bash
chronyd -Q 'server <TIME_SERVER_IP> iburst'
```

To compare the time server against public NTP and see how close it is:

```bash
chronyc sourcestats
```

On the **time server** itself, check PPS precision:

```bash
chronyc tracking
```

Key fields:
- **System time** — offset from NTP time (should be nanoseconds)
- **Root delay** — round-trip to reference (0.000 for local PPS)
- **Root dispersion** — estimated error bound

For continuous monitoring:

```bash
watch -n 1 'chronyc tracking'
```

## 8. Teardown

Stop and disable all services:

```bash
sudo systemctl stop chrony
sudo systemctl disable chrony
sudo systemctl stop gpsd
sudo systemctl disable gpsd
```

Optionally stop the bridge daemon:

```bash
sudo systemctl stop jpgnss2gpsd-bridge
sudo systemctl disable jpgnss2gpsd-bridge
```

Remove gpsd systemd override:

```bash
sudo rm -rf /etc/systemd/system/gpsd.service.d
sudo systemctl daemon-reload
```

Restore original chrony config:

```bash
sudo cp /etc/chrony/chrony.conf.backup /etc/chrony/chrony.conf
```

Disable PPS — remove `dtoverlay=pps-gpio,gpiopin=5` from `/boot/firmware/config.txt`, then:

```bash
sudo rmmod pps-gpio
sudo sed -i '/^pps-gpio$/d' /etc/modules
sudo reboot
```

After reboot, `/dev/pps0` will no longer exist and GPIO 5 is free for `IGnssHat::timepulse()` again.

## Troubleshooting

**No `/dev/pps0`:**
```bash
dmesg | grep pps
# Check config.txt has dtoverlay=pps-gpio,gpiopin=5
# Reboot if just added
```

**`ppstest` shows no pulses:**
- GNSS module needs a fix first — check antenna, go outside
- Verify timepulse is enabled in GNSS config (default: enabled)

**chrony shows `?` for PPS:**
```bash
# PPS needs NMEA lock first — wait for gpsd to get a fix
cgps  # wait until fix shows up
chronyc sources -v  # re-check after fix
```

**gpsd can't open `/dev/jimmypaputto/gnss`:**
```bash
sudo systemctl restart jpgnss2gpsd-bridge
ls /dev/jimmypaputto/gnss
```

**Monitor in real time:**
```bash
watch -n 1 'chronyc tracking'
```
