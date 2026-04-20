#!/bin/bash
#
# Jimmy Paputto GNSS Bridge Daemon Uninstallation Script
#

set -e

DAEMON_NAME="jpgnss2gpsd-bridge"
BINARY_PATH="/usr/local/bin/${DAEMON_NAME}"
SERVICE_PATH="/etc/systemd/system/${DAEMON_NAME}.service"

echo "=============================================="
echo " Jimmy Paputto GNSS Bridge Daemon Uninstaller "
echo "=============================================="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ ERROR: This script must be run as root (use sudo)"
   echo "   Usage: sudo ./uninstall_daemon.sh"
   exit 1
fi

echo "🗑️  Uninstalling ${DAEMON_NAME} daemon..."

# Stop and disable service
if systemctl is-enabled --quiet "${DAEMON_NAME}" 2>/dev/null; then
    echo "🛑 Stopping and disabling ${DAEMON_NAME} service..."
    systemctl stop "${DAEMON_NAME}" || true
    systemctl disable "${DAEMON_NAME}" || true
fi

# Remove service file
if [ -f "${SERVICE_PATH}" ]; then
    echo "🗑️  Removing service file..."
    rm -f "${SERVICE_PATH}"
fi

# Remove binary
if [ -f "${BINARY_PATH}" ]; then
    echo "🗑️  Removing binary..."
    rm -f "${BINARY_PATH}"
fi

# Reload systemd
echo "🔄 Reloading systemd daemon..."
systemctl daemon-reload

# Clean up virtual device (if exists)
if [ -L "/dev/jimmypaputto/gnss" ]; then
    echo "🧹 Cleaning up virtual device..."
    rm -f "/dev/jimmypaputto/gnss"
    rmdir "/dev/jimmypaputto" 2>/dev/null || true
fi

echo ""
echo "🎉 Uninstallation completed successfully!"
echo "   ${DAEMON_NAME} has been completely removed from the system."
echo ""
