#!/bin/bash
# Uninstall gnsshat-rtk-base systemd service.
# Must be run as root.
#
# Usage:
#   sudo ./uninstall-rtk-base-service.sh          # keep config
#   sudo ./uninstall-rtk-base-service.sh --purge   # remove config and user

set -e

SERVICE_NAME="gnsshat-rtk-base"
SERVICE_USER="gnsshat"
CONFIG_DIR="/etc/gnsshat"
CONFIG_FILE="${CONFIG_DIR}/rtk-base.toml"
UNIT_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
PURGE=false

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: must be run as root" >&2
    exit 1
fi

for arg in "$@"; do
    case "$arg" in
        --purge) PURGE=true ;;
    esac
done

# ── Stop and disable service ─────────────────────────────────────
if systemctl is-active --quiet "${SERVICE_NAME}" 2>/dev/null; then
    systemctl stop "${SERVICE_NAME}"
    echo "Stopped ${SERVICE_NAME}"
fi

if systemctl is-enabled --quiet "${SERVICE_NAME}" 2>/dev/null; then
    systemctl disable "${SERVICE_NAME}"
    echo "Disabled ${SERVICE_NAME}"
fi

# ── Remove unit file ─────────────────────────────────────────────
if [ -f "${UNIT_FILE}" ]; then
    rm "${UNIT_FILE}"
    echo "Removed ${UNIT_FILE}"
fi

systemctl daemon-reload

# ── Purge config and user if requested ───────────────────────────
if [ "${PURGE}" = true ]; then
    if [ -d "${CONFIG_DIR}" ]; then
        rm -rf "${CONFIG_DIR}"
        echo "Removed ${CONFIG_DIR}"
    fi
    if id -u "${SERVICE_USER}" >/dev/null 2>&1; then
        userdel "${SERVICE_USER}" 2>/dev/null || true
        echo "Removed user ${SERVICE_USER}"
    fi
    echo "Purge complete."
else
    if [ -f "${CONFIG_FILE}" ]; then
        echo "Config preserved: ${CONFIG_FILE}"
    fi
    echo "Run with --purge to also remove config and user."
fi

echo "Done."
