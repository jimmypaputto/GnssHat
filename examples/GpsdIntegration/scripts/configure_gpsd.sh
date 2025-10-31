#!/bin/bash
#
# Auto-configure gpsd as systemd service with jpgnss2gpsd-bridge
#

set -e

echo "=============================================="
echo "GPSD + JP GNSS2GPSD Bridge Auto Configuration"
echo "=============================================="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ ERROR: This script must be run as root (use sudo)"
   echo "   Usage: sudo ./configure_gpsd.sh [--with-pps]"
   exit 1
fi

USE_PPS=false
if [[ "$1" == "--with-pps" ]]; then
    USE_PPS=true
    echo "🔔 Configuring GPSD with PPS support"
else
    echo "📡 Configuring GPSD without PPS"
fi

# Install gpsd if not present
if ! command -v gpsd &> /dev/null; then
    echo "📦 Installing gpsd..."
    apt-get update
    apt-get install -y gpsd gpsd-clients
fi

# Create gpsd systemd override
echo "⚙️  Configuring gpsd systemd service..."

GPSD_SERVICE_DIR="/etc/systemd/system"
GPSD_SERVICE_OVERRIDE_DIR="${GPSD_SERVICE_DIR}/gpsd.service.d"
GPSD_CONFIG_FILE="/etc/default/gpsd"

# Create gpsd configuration file
if [ "$USE_PPS" = true ]; then
    # Check if PPS device exists
    if [ ! -c "/dev/pps0" ]; then
        echo "⚠️  WARNING: /dev/pps0 not found. Make sure PPS is properly configured."
        echo "   Add to /boot/firmware/config.txt: dtoverlay=pps-gpio,gpiopin=5"
        echo "   Then reboot and run: sudo modprobe pps-gpio"
    fi
    
    cat > "${GPSD_CONFIG_FILE}" << 'EOF'
# Configuration for gpsd with Jimmy Paputto GNSS HAT + PPS
DEVICES="/dev/jimmypaputto/gnss /dev/pps0"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
else
    cat > "${GPSD_CONFIG_FILE}" << 'EOF'
# Configuration for gpsd with Jimmy Paputto GNSS HAT
DEVICES="/dev/jimmypaputto/gnss"
GPSD_OPTIONS="-n"
USBAUTO="false"
START_DAEMON="true"
GPSD_SOCKET="/var/run/gpsd.sock"
EOF
fi

# Create systemd override for dependency management
mkdir -p "${GPSD_SERVICE_OVERRIDE_DIR}"
cat > "${GPSD_SERVICE_OVERRIDE_DIR}/jimmy-paputto.conf" << 'EOF'
[Unit]
# Require Jimmy Paputto bridge to be running first
Requires=jpgnss2gpsd-bridge.service
After=jpgnss2gpsd-bridge.service
ConditionPathExists=/dev/jimmypaputto/gnss

[Service]
# Auto-restart on failure
Restart=always
RestartSec=5
EOF

# Reload systemd
echo "🔄 Reloading systemd..."
systemctl daemon-reload

# Enable services
echo "✅ Enabling services..."
systemctl enable jpgnss2gpsd-bridge
systemctl enable gpsd

echo ""
echo "🎉 Configuration completed successfully!"
echo ""
echo "Configuration files created:"
echo "  GPSD config: /etc/default/gpsd"
echo "  Systemd override: /etc/systemd/system/gpsd.service.d/jimmy-paputto.conf"
echo ""
echo "Service dependencies configured:"
echo "  jpgnss2gpsd-bridge → creates /dev/jimmypaputto/gnss"
if [ "$USE_PPS" = true ]; then
    echo "  gpsd → uses /dev/jimmypaputto/gnss + /dev/pps0"
else
    echo "  gpsd → uses /dev/jimmypaputto/gnss"
fi
echo ""
echo "To start the complete GPS solution:"
echo "  sudo systemctl start jpgnss2gpsd-bridge  # Bridge starts first"
echo "  sudo systemctl start gpsd                # GPSD waits for bridge"
echo ""
echo "Or start both with dependency handling:"
echo "  sudo systemctl start gpsd  # Automatically starts bridge first"
echo ""
echo "To view GPS data:"
echo "  cgps"
echo "  gpsmon"
echo ""
if [ "$USE_PPS" = true ]; then
    echo "PPS verification:"
    echo "  sudo ppstest /dev/pps0     # Check PPS pulses"
    echo "  gpsmon                     # Look for PPS indicator"
    echo ""
    echo "Time server integration:"
    echo "  See README.md for ntpd/chrony configuration"
fi
