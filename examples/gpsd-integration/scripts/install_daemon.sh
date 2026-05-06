#!/bin/bash
#
# Jimmy Paputto GNSS Bridge Daemon Installation Script
# 
# This script installs jpgnss-bridge as a systemd service
#

set -e

DAEMON_NAME="jpgnss2gpsd-bridge"
BINARY_PATH="/usr/local/bin/jimmypaputto/${DAEMON_NAME}"
SERVICE_PATH="/etc/systemd/system/${DAEMON_NAME}.service"
BUILD_DIR="$(dirname "$0")/../build"
SERVICE_FILE="$(dirname "$0")/../res/${DAEMON_NAME}.service"

echo "=============================================="
echo "  Jimmy Paputto GNSS Bridge Daemon Installer  "
echo "=============================================="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "❌ ERROR: This script must be run as root (use sudo)"
   echo "   Usage: sudo ./install_daemon.sh"
   exit 1
fi

# Check if binary exists
if [ ! -f "${BUILD_DIR}/jpgnss2gpsd-bridge" ]; then
    echo "❌ ERROR: Binary not found at ${BUILD_DIR}/jpgnss2gpsd-bridge"
    echo "   Please build the project first:"
    echo "   cd $(dirname "$0")/../build && make -j4"
    exit 1
fi

# Check if service file exists
if [ ! -f "${SERVICE_FILE}" ]; then
    echo "❌ ERROR: Service file not found at ${SERVICE_FILE}"
    exit 1
fi

echo "📦 Installing ${DAEMON_NAME} daemon..."

# Stop service if running
if systemctl is-active --quiet "${DAEMON_NAME}"; then
    echo "🛑 Stopping existing ${DAEMON_NAME} service..."
    systemctl stop "${DAEMON_NAME}"
fi

# Copy binary
echo "📁 Installing binary to ${BINARY_PATH}..."
cp "${BUILD_DIR}/jpgnss2gpsd-bridge" "${BINARY_PATH}"
chmod +x "${BINARY_PATH}"

# Copy service file
echo "⚙️  Installing systemd service..."
cp "${SERVICE_FILE}" "${SERVICE_PATH}"

# Copy documentation
echo "📄 Copying documentation..."
DOC_DIR="/usr/local/share/doc/jimmypaputto/${DAEMON_NAME}"
cp -r "$(dirname "$0")/../README.md" "${DOC_DIR}"

# Reload systemd
echo "🔄 Reloading systemd daemon..."
systemctl daemon-reload

# Enable service
echo "✅ Enabling ${DAEMON_NAME} service..."
systemctl enable "${DAEMON_NAME}"

echo ""
echo "🎉 Installation completed successfully!"
echo ""
echo "Usage commands:"
echo "  sudo systemctl start ${DAEMON_NAME}     # Start the daemon"
echo "  sudo systemctl stop ${DAEMON_NAME}      # Stop the daemon"
echo "  sudo systemctl status ${DAEMON_NAME}    # Check status"
echo "  sudo systemctl restart ${DAEMON_NAME}   # Restart the daemon"
echo "  sudo journalctl -u ${DAEMON_NAME} -f    # View logs"
echo ""
echo "To start automatically:"
echo "  sudo systemctl enable ${DAEMON_NAME}"
echo ""
echo "GPSD Integration:"
echo "  sudo gpsd -N -D5 /dev/jimmypaputto/gnss"
echo "  cgps  # View GPS data"
echo ""
