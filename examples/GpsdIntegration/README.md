# JP GNSS to GPSD Bridge Daemon

Systemd daemon that bridges Jimmy Paputto GNSS HAT data to gpsd via virtual serial port.

## Quick Start

### 1. Build
```bash
mkdir -p build
cd build
cmake ..
make
```

### 2. Install as systemd service
```bash
# Install daemon by script
sudo ./scripts/install_daemon.sh

# Or manually
cd build && sudo make install
systemctl daemon-reload
sudo systemctl enable jpgnss2gpsd-bridge

# Start daemon
sudo systemctl start jpgnss2gpsd-bridge

# Check status
sudo systemctl status jpgnss2gpsd-bridge
```

### 3. Quick use with gpsd
```bash
# Start gpsd in terminal
sudo gpsd -N -D5 /dev/jimmypaputto/gnss

# View GPS data
cgps
gpsmon
```

## Daemon Control

### Using systemctl directly:
```bash
sudo systemctl start jpgnss2gpsd-bridge
sudo systemctl stop jpgnss2gpsd-bridge
sudo systemctl status jpgnss2gpsd-bridge
sudo systemctl restart jpgnss2gpsd-bridge
sudo journalctl -u jpgnss2gpsd-bridge -f
```

## Virtual Device

Daemon creates: `/dev/jimmypaputto/gnss`
- Emulates serial GPS device
- 9600 baud, 8N1
- Sends NMEA sentences (GGA, RMC, GSA)
- Updates every 1 second

## Integration Examples

### PPS (if pps is enabled you can not use timepulse functionalities from library like IGnssHat::timepulse())

#### Step 1: Enable PPS in device tree
```bash
# Add to /boot/frimware/config.txt (requires reboot):
dtoverlay=pps-gpio,gpiopin=5
```

#### Step 2: Load PPS kernel module (afer reboot)
```bash
# Load module
sudo modprobe pps-gpio

# Make it permanent - add to /etc/modules:
echo "pps-gpio" | sudo tee -a /etc/modules

# Verify PPS device exists
ls -la /dev/pps*
# Should show: /dev/pps0
```

#### Step 3: Test PPS signal
```bash
# Install pps-tools
sudo apt-get install pps-tools

# Test PPS signal (should show timestamps)
sudo ppstest /dev/pps0
# Expected output:
# trying PPS source "/dev/pps0"
# found PPS source "/dev/pps0"
# ok, found 1 source(s), now start fetching data...
# source 0 - assert 1234567890.123456789, sequence: 1
```

### GPSD with PPS (Full systemd integration):
```bash
# Option A: Auto-configure with script (RECOMMENDED)
sudo ./scripts/configure_gpsd.sh --with-pps

# Option B: Manual configuration  
# 1. Start GNSS bridge daemon
sudo systemctl start jpgnss2gpsd-bridge
sudo systemctl enable jpgnss2gpsd-bridge

# 2. Install gpsd
sudo apt-get install gpsd gpsd-clients

# 3. Configure gpsd via its config file
sudo nano /etc/default/gpsd
# Add these lines:
# DEVICES="/dev/jimmypaputto/gnss /dev/pps0"
# GPSD_OPTIONS="-n"
# USBAUTO="false"
# START_DAEMON="true"
# GPSD_SOCKET="/var/run/gpsd.sock"

# 4. Create systemd dependency (gpsd starts after bridge)
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

# 5. Enable and start services
sudo systemctl daemon-reload
sudo systemctl enable jpgnss2gpsd-bridge
sudo systemctl enable gpsd
sudo systemctl start jpgnss2gpsd-bridge
sudo systemctl start gpsd

# Verify both are running
sudo systemctl status jpgnss2gpsd-bridge
sudo systemctl status gpsd

# Check GPS data with PPS
cgps       # Should show GPS fix
gpsmon     # Should show PPS pulses
```

### Time Synchronization with GNSS

Modern systems require precise time synchronization. There are two main NTP implementations:

#### **NTP vs Chrony - Which to choose?**

**NTP (Network Time Protocol daemon):**
- ✅ **Traditional** - oldest and most established
- ✅ **Widely supported** - works everywhere
- ✅ **Stable** - proven in production environments
- ❌ **Slower convergence** - takes longer to sync
- ❌ **Less accurate** - ~1ms precision
- 🎯 **Best for:** Servers, traditional setups, compatibility

**Chrony:**
- ✅ **Modern** - designed for mobile/intermittent connections
- ✅ **Fast convergence** - syncs quickly after network outages
- ✅ **High accuracy** - sub-millisecond precision
- ✅ **Better PPS support** - optimized for hardware clocks
- ❌ **Newer** - less widely deployed
- 🎯 **Best for:** Laptops, Raspberry Pi, high-precision applications

**Recommendation:** Use **Chrony** for Raspberry Pi + GNSS applications!

### With chrony + PPS (Modern, recommended):
```bash
# Option A: Auto-configure with script (RECOMMENDED)
sudo ./scripts/configure_chrony.sh --with-pps

# Option B: Manual configuration
# 1. Ensure gpsd is running with PPS
sudo systemctl start jpgnss2gpsd-bridge
sudo systemctl start gpsd

# 2. Install chrony (remove ntp if present)
sudo systemctl stop ntp || true
sudo systemctl disable ntp || true
sudo apt-get remove ntp -y || true
sudo apt-get install chrony

# 3. Backup original config
sudo cp /etc/chrony/chrony.conf /etc/chrony/chrony.conf.backup

# 4. Configure chrony for GNSS + PPS
sudo tee /etc/chrony/chrony.conf > /dev/null <<EOF
# Jimmy Paputto GNSS HAT + PPS Configuration

# GPS time via SHM from gpsd
refclock SHM 0 offset 0.0 delay 0.2 refid NMEA

# PPS (ultra-precise timing)
refclock PPS /dev/pps0 refid PPS lock NMEA

# Fallback internet time servers
pool 0.pool.ntp.org iburst
pool 1.pool.ntp.org iburst

# Local network access (adjust for your network) for example
# allow 192.168.0.0/16

# Performance tuning for GNSS
maxupdateskew 100.0
makestep 1000 3
rtcsync

# Logging and statistics
log tracking measurements statistics
logdir /var/log/chrony
EOF

# 5. Create log directory
sudo mkdir -p /var/log/chrony
sudo chown chrony:chrony /var/log/chrony

# 6. Enable and start chrony
sudo systemctl enable chrony
sudo systemctl restart chrony

# 7. Verify configuration
chronyc sources -v     # Show time sources
chronyc tracking       # Show synchronization status
chronyc sourcestats    # Show source statistics
```
### With ntpd + PPS (Traditional setup):
```bash
# Option A: Auto-configure with script (RECOMMENDED)
sudo ./scripts/configure_ntp.sh --with-pps

# Option B: Manual configuration
# 1. Ensure gpsd is running with PPS
sudo systemctl start jpgnss2gpsd-bridge
sudo systemctl start gpsd

# 2. Install ntp (remove chrony if present)
sudo systemctl stop chronyd || true
sudo systemctl disable chronyd || true
sudo apt-get remove chrony -y || true
sudo apt-get install ntp

# 3. Backup original config
sudo cp /etc/ntp.conf /etc/ntp.conf.backup

# 4. Configure ntp for GNSS + PPS
sudo tee /etc/ntp.conf > /dev/null <<EOF
# Jimmy Paputto GNSS HAT + PPS Configuration

# Use local GPS time via gpsd shared memory
server 127.127.28.0 minpoll 4 maxpoll 4
fudge 127.127.28.0 time1 0.420 refid GPS stratum 1

# Use PPS signal for high precision
server 127.127.22.0 minpoll 4 maxpoll 4 prefer
fudge 127.127.22.0 refid PPS stratum 0

# Fallback internet time servers
pool 0.pool.ntp.org iburst
pool 1.pool.ntp.org iburst

# Security and access control
restrict default kod nomodify notrap nopeer noquery
restrict 127.0.0.1
restrict ::1

# Statistics
statsdir /var/log/ntpstats/
statistics loopstats peerstats clockstats
filegen loopstats file loopstats type day enable
filegen peerstats file peerstats type day enable
filegen clockstats file clockstats type day enable
EOF

# 5. Create stats directory
sudo mkdir -p /var/log/ntpstats
sudo chown ntp:ntp /var/log/ntpstats

# 6. Enable and start ntp
sudo systemctl enable ntp
sudo systemctl restart ntp

# 7. Verify configuration
ntpq -p  # Should show GPS and PPS sources
ntpq -c rv  # Show system status
```

## Troubleshooting

### Permission issues:
```bash
# Run with sudo - daemon needs root for /dev access
sudo ./scripts/jpgnss2gpsd-bridge.sh start
```

### Check logs:
```bash
./scripts/jpgnss2gpsd-bridge.sh logs
```

### Manual testing:
```bash
# Test binary directly
sudo ./build/jpgnss2gpsd-bridge

# Test virtual device
cat /dev/jimmypaputto/gnss
```

### Cleanup:
```bash
# Complete removal
sudo ./scripts/uninstall_daemon.sh
```

### PPS Troubleshooting:

#### No /dev/pps0 device:
```bash
# Check device tree overlay
dtoverlay -l | grep pps

# Check kernel messages
dmesg | grep pps

# Manually load module with debug
sudo modprobe pps-gpio gpio_pin=18
```

#### PPS signal not working:
```bash
# Check GPIO connection (should be GPIO 18 or GPIO 5)
# Verify 1PPS signal with oscilloscope/logic analyzer

# Check ppstest output
sudo ppstest /dev/pps0

# If no pulses, check GNSS configuration:
# - Timepulse must be enabled in library config
# - Check GPIO pin mapping
```

#### gpsd not seeing PPS:
```bash
# Check gpsd logs
sudo journalctl -u gpsd -f

# Verify PPS permissions
ls -la /dev/pps0
sudo chmod 666 /dev/pps0

# Test manual gpsd start
sudo gpsd -N -D5 -S 2222 /dev/jimmypaputto/gnss /dev/pps0
```

### Time Synchronization Troubleshooting:

#### NTP not syncing:
```bash
# Check ntp status
ntpq -p
ntpstat

# Look for GPS/PPS sources
# Should show:
# *GPS(0) - GPS active (asterisk means selected)
# oPPS(0) - PPS available (o means PPS peer)

# Check ntp logs
sudo journalctl -u ntp -f

# Manual sync test
sudo ntpdate -s time.nist.gov
```

#### Chrony not syncing:
```bash
# Check chrony status
chronyc sources -v
chronyc tracking

# Look for GPS/PPS sources
# Should show:
# #* GPS - GPS synced (# means local, * means selected)
# #* PPS - PPS synced and preferred

# Check chrony logs
sudo journalctl -u chrony -f

# Force online sources
chronyc online
```

#### No GPS time source:
```bash
# Verify gpsd is providing shared memory
ipcs -m | grep 28
# Should show shared memory segments

# Check gpsd shared memory
gpsmon  # Look for "SHM" or shared memory output

# Test gpsd JSON output
echo "?WATCH={\"enable\":true,\"json\":true}" | nc localhost 2947
```

#### PPS not working with time sync:
```bash
# Verify PPS device permissions
ls -la /dev/pps0
sudo chmod 666 /dev/pps0

# Check PPS signal
sudo ppstest /dev/pps0

# For NTP - check kernel PPS
dmesg | grep pps

# For Chrony - verify PPS refclock
chronyc sources | grep PPS
```

## Features

- Systemd integration
- Easy install/uninstall
- NMEA 0183 compliant output
- 1PPS timepulse support
- Compatible with all GPS software
