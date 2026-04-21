#!/bin/bash
# Install gnsshat-rtk-base as a systemd service.
# Must be run as root.

set -e

SERVICE_NAME="gnsshat-rtk-base"
SERVICE_USER="gnsshat"
CONFIG_DIR="/etc/gnsshat"
CONFIG_FILE="${CONFIG_DIR}/rtk-base.toml"
UNIT_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: must be run as root" >&2
    exit 1
fi

# ── Create dedicated system user ────────────────────────────────
if ! id -u "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd --system --no-create-home --shell /usr/sbin/nologin "${SERVICE_USER}"
    echo "Created system user: ${SERVICE_USER}"
else
    echo "User ${SERVICE_USER} already exists"
fi

# Add to device-access groups
usermod -aG spi,gpio,dialout "${SERVICE_USER}" 2>/dev/null || true

# ── Deploy config file (no-clobber) ─────────────────────────────
mkdir -p "${CONFIG_DIR}"

if [ ! -f "${CONFIG_FILE}" ]; then
    if [ -f "${SCRIPT_DIR}/example-rtk-base.toml" ]; then
        cp "${SCRIPT_DIR}/example-rtk-base.toml" "${CONFIG_FILE}"
        chmod 644 "${CONFIG_FILE}"
        echo "Installed default config: ${CONFIG_FILE}"
    else
        echo "Warning: example-rtk-base.toml not found, skipping config install" >&2
    fi
else
    echo "Config already exists: ${CONFIG_FILE} (not overwritten)"
fi

# ── Install service unit ─────────────────────────────────────────
if [ -f "${SCRIPT_DIR}/res/${SERVICE_NAME}.service" ]; then
    cp "${SCRIPT_DIR}/res/${SERVICE_NAME}.service" "${UNIT_FILE}"
    chmod 644 "${UNIT_FILE}"
    echo "Installed service: ${UNIT_FILE}"
else
    echo "Error: ${SCRIPT_DIR}/res/${SERVICE_NAME}.service not found" >&2
    exit 1
fi

# ── Enable service ───────────────────────────────────────────────
systemctl daemon-reload
systemctl enable "${SERVICE_NAME}"
echo "Service enabled: ${SERVICE_NAME}"

# ── Verify binary is installed ───────────────────────────────────
if ! command -v gnsshat-rtk-base >/dev/null 2>&1; then
    echo ""
    echo "Warning: gnsshat-rtk-base not found in PATH."
    echo "Make sure to build and install it first:"
    echo "  cmake --build build && sudo cmake --install build"
fi

echo ""
echo "Done. Next steps:"
echo "  1. Edit config:  sudo nano ${CONFIG_FILE}"
echo "  2. Start service: sudo systemctl start ${SERVICE_NAME}"
echo "  3. View logs:     journalctl -u ${SERVICE_NAME} -f"
echo ""
echo "Tip: to customize the unit without editing the shipped file, create a drop-in:"
echo "  sudo systemctl edit ${SERVICE_NAME}"
echo "  # add e.g.  [Service]\n  # ExecStart=\n  # ExecStart=/usr/local/bin/gnsshat-rtk-base --config ${CONFIG_FILE} --no-watchdog"
echo "Then: sudo systemctl daemon-reload && sudo systemctl restart ${SERVICE_NAME}"
