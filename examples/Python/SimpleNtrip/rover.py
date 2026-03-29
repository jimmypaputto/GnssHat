# Jimmy Paputto 2026

#
# SimpleNtrip — RTK Rover with NTRIP Client
#
# Connects to the NTRIP caster run by base.py, receives RTCM3
# correction data, and applies it to the local GNSS HAT for
# centimeter-level RTK positioning.
#
# Usage:
#   1. Edit config_rover.json — set caster_ip to the IP of the base Pi
#   2. Make sure base.py is already running on the base Pi
#   3. Run: python rover.py
#

import json
import signal
import sys
import logging
from pathlib import Path
from queue import Queue, Empty
from threading import Event, Thread

from jimmypaputto import gnsshat
from pygnssutils import GNSSNTRIPClient

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("rover")

CONFIG_PATH = Path(__file__).parent / "config_rover.json"


def load_config() -> dict:
    with open(CONFIG_PATH) as f:
        return json.load(f)


def create_rover_config() -> dict:
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.PORTABLE,
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
            'mode': gnsshat.RtkMode.ROVER
        }
    }


class NtripRover:

    def __init__(self, cfg: dict):
        self._cfg = cfg
        self._stop_event = Event()
        self._rtcm_queue: Queue = Queue()
        self._ntrip_client: GNSSNTRIPClient | None = None
        self._hat: gnsshat.GnssHat | None = None

    def start_gnss(self) -> bool:
        self._hat = gnsshat.GnssHat()
        self._hat.soft_reset_hot_start()

        if not self._hat.start(create_rover_config()):
            log.error("Failed to start GNSS receiver.")
            return False

        log.info("GNSS receiver started in RTK Rover mode.")
        return True

    def start_ntrip(self) -> bool:
        cfg = self._cfg
        server = cfg["caster_ip"]
        port = cfg.get("caster_port", 2101)
        mountpoint = cfg.get("mountpoint", "MY_RTK_BASE")
        version = cfg.get("ntrip_version", "2.0")

        log.info(f"Connecting to NTRIP caster: "
                 f"{server}:{port}/{mountpoint}")

        self._ntrip_client = GNSSNTRIPClient()

        streaming = self._ntrip_client.run(
            server=server,
            port=port,
            https=0,
            mountpoint=mountpoint,
            datatype="RTCM",
            version=version,
            ntripuser="",
            ntrippassword="",
            ggainterval=-1,
            ggamode=0,
            output=self._rtcm_queue,
            stopevent=self._stop_event,
        )

        if not streaming:
            log.error("NTRIP connection failed.")
            return False

        log.info("NTRIP client connected — streaming RTCM3 corrections.\n")
        return True

    def _apply_corrections_loop(self):
        while not self._stop_event.is_set():
            frames = []
            try:
                raw, _parsed = self._rtcm_queue.get(timeout=1.0)
                frames.append(raw)
            except Empty:
                continue

            while not self._rtcm_queue.empty():
                try:
                    raw, _parsed = self._rtcm_queue.get_nowait()
                    frames.append(raw)
                except Empty:
                    break

            if frames:
                self._hat.rtk_apply_corrections(frames)

    def run(self):
        if not self.start_gnss():
            return -1
        if not self.start_ntrip():
            return -1

        corrections_thread = Thread(
            target=self._apply_corrections_loop,
            daemon=True,
        )
        corrections_thread.start()

        log.info("Monitoring RTK fix quality (Ctrl+C to stop)...\n")

        try:
            while not self._stop_event.is_set():
                nav = self._hat.wait_and_get_fresh_navigation()
                fix_quality = gnsshat.FixQuality(nav.pvt.fix_quality)
                fix_type = gnsshat.FixType(nav.pvt.fix_type)

                lat = nav.pvt.latitude
                lon = nav.pvt.longitude
                alt = nav.pvt.altitude_msl
                hacc = nav.pvt.horizontal_accuracy
                vacc = nav.pvt.vertical_accuracy

                print(
                    f"\033[1m[{gnsshat.utc_time_iso8601(nav.pvt)}]\033[0m "
                    f"Fix: {fix_quality.name} ({fix_type.name})  "
                    f"Pos: {lat:.8f}, {lon:.8f}, {alt:.2f} m  "
                    f"Acc: H={hacc:.3f} m, V={vacc:.3f} m"
                )

        except KeyboardInterrupt:
            log.info("\nStopping RTK Rover...")
        except Exception as e:
            log.error(f"Error: {e}")
            return -1
        finally:
            self.stop()

        log.info("Rover stopped.")
        return 0

    def stop(self):
        self._stop_event.set()
        if self._ntrip_client is not None:
            self._ntrip_client.stop()


def main():
    cfg = load_config()
    rover = NtripRover(cfg)

    def signal_handler(_sig, _frame):
        rover.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    return rover.run()


if __name__ == "__main__":
    sys.exit(main())
