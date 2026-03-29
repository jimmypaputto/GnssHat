# Jimmy Paputto 2026

#
# SimpleNtrip — RTK Base Station with NTRIP Caster
#
# Configures the GNSS HAT as an RTK Base station, starts a local
# NTRIP caster, and broadcasts RTCM3 correction data to any
# connected rover over the network.
#
# Usage:
#   1. Edit config_base.json (set base_mode, survey-in params, etc.)
#   2. Run: python base.py
#   3. On the rover Pi, run: python rover.py
#

import json
import signal
import sys
import logging
from pathlib import Path

from jimmypaputto import gnsshat
from ntrip_caster import NtripCaster

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("base")

CONFIG_PATH = Path(__file__).parent / "config_base.json"


def load_config() -> dict:
    with open(CONFIG_PATH) as f:
        return json.load(f)


def build_gnss_config(cfg: dict) -> dict:
    """Build the GnssHat config dict from the JSON config."""
    base_mode = cfg.get("base_mode", "survey_in")

    if base_mode == "fixed_position":
        fp = cfg["fixed_position"]
        base_cfg = {
            'base_mode': gnsshat.BaseMode.FIXED_POSITION,
            'fixed_position': {
                'position_type': gnsshat.FixedPositionType.LLA,
                'lla': {
                    'latitude_deg': fp["latitude_deg"],
                    'longitude_deg': fp["longitude_deg"],
                    'height_m': fp["height_m"],
                },
                'position_accuracy_m': fp.get("position_accuracy_m", 0.5),
            }
        }
    else:
        si = cfg.get("survey_in", {})
        base_cfg = {
            'base_mode': gnsshat.BaseMode.SURVEY_IN,
            'survey_in': {
                'minimum_observation_time_s':
                    si.get("minimum_observation_time_s", 120),
                'required_position_accuracy_m':
                    si.get("required_position_accuracy_m", 50.0),
            }
        }

    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'pulse_when_no_fix': None,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None,
        'rtk': {
            'mode': gnsshat.RtkMode.BASE,
            'base': base_cfg,
        }
    }


def get_rtcm3_frame_id(frame: bytes) -> int:
    if len(frame) < 6 or frame[0] != 0xD3:
        return 0
    return (frame[3] << 4) | (frame[4] >> 4)


def main():
    cfg = load_config()

    # --- Start GNSS as RTK Base ---
    hat = gnsshat.GnssHat()
    hat.soft_reset_hot_start()

    gnss_config = build_gnss_config(cfg)
    base_mode = cfg.get("base_mode", "survey_in")

    if not hat.start(gnss_config):
        log.error("Failed to start GNSS receiver.")
        return -1

    log.info(f"GNSS started as RTK Base ({base_mode}).")

    # --- Start NTRIP Caster ---
    caster = NtripCaster(
        host=cfg.get("caster_ip", "0.0.0.0"),
        port=cfg.get("caster_port", 2101),
        mountpoint=cfg.get("mountpoint", "MY_RTK_BASE"),
    )
    caster.start()

    stop_requested = False

    def signal_handler(_sig, _frame):
        nonlocal stop_requested
        stop_requested = True

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    log.info("Waiting for Survey-In / corrections...\n")

    total_frames_sent = 0

    try:
        while not stop_requested:
            nav = hat.wait_and_get_fresh_navigation()
            fix_type = gnsshat.FixType(nav.pvt.fix_type)

            if fix_type != gnsshat.FixType.TIME_ONLY_FIX:
                log.info(f"Fix: {fix_type.name} — "
                         f"waiting for TIME_ONLY_FIX...")
                continue

            caster.update_position(nav.pvt.latitude, nav.pvt.longitude)

            try:
                corrections = hat.rtk_get_full_corrections()
            except RuntimeError as e:
                log.warning(f"Corrections not available: {e}")
                continue

            if not corrections:
                continue

            caster.feed(corrections)
            total_frames_sent += len(corrections)

            ids = [get_rtcm3_frame_id(f) for f in corrections]
            log.info(
                f"[{gnsshat.utc_time_iso8601(nav.pvt)}] "
                f"Sent {len(corrections)} RTCM3 frames "
                f"(IDs: {ids}) | "
                f"Clients: {caster.client_count} | "
                f"Total: {total_frames_sent}"
            )

    except Exception as e:
        log.error(f"Error: {e}")
        return -1
    finally:
        caster.stop()
        log.info("Base station stopped.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
