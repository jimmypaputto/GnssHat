# Jimmy Paputto 2025

# RTK Rover example with NTRIP Client (Python)
#
# Demonstrates configuring the GNSS module as an RTK Rover that
# receives RTCM3 correction data from an NTRIP caster over the
# internet and applies it to the receiver for centimeter-level
# positioning accuracy.
#
# Uses pygnssutils (GNSSNTRIPClient) for NTRIP communication.


import signal
import sys
from queue import Queue, Empty
from threading import Event, Thread

from jimmypaputto import gnsshat
from pygnssutils import GNSSNTRIPClient


# ============================================================
# NTRIP Caster Configuration — EDIT THESE FOR YOUR SETUP
# ============================================================

NTRIP_CASTER_IP = "caster.ip"             # NTRIP caster hostname or IP
NTRIP_PORT = 8086                         # NTRIP caster port
NTRIP_MOUNTPOINT = "NEAREST"              # Mount point name
NTRIP_USER = "user"                       # NTRIP account username
NTRIP_PASSWORD = "password"               # NTRIP account password
NTRIP_VERSION = "2.0"                     # NTRIP protocol: "1.0" or "2.0"
NTRIP_HTTPS = False                       # Use HTTPS (TLS) connection

# GGA position reporting — sends your position to the caster
# so it can select the best base station / generate VRS.
# Set GGA_INTERVAL = -1 to disable GGA reporting.
GGA_INTERVAL = -1                         # Seconds between GGA messages (-1 = off)
GGA_MODE = 0                              # 0 = live from receiver, 1 = fixed reference
REFERENCE_LAT = 0.0                       # Fixed reference latitude  (if GGA_MODE=1)
REFERENCE_LON = 0.0                       # Fixed reference longitude (if GGA_MODE=1)
REFERENCE_ALT = 0.0                       # Fixed reference altitude  (if GGA_MODE=1)
REFERENCE_SEP = 0.0                       # Fixed reference separation (if GGA_MODE=1)

# ============================================================


def create_config() -> dict:
    """Create rover configuration"""
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
            'mode': gnsshat.RtkMode.ROVER
        }
    }


class NtripRover:
    """
    RTK Rover that connects to an NTRIP caster, receives RTCM3
    corrections, and applies them to the GNSS receiver.
    """

    def __init__(self):
        self._stop_event = Event()
        self._rtcm_queue: Queue = Queue()
        self._ntrip_client: GNSSNTRIPClient | None = None
        self._hat: gnsshat.GnssHat | None = None

    def start_gnss(self) -> bool:
        """Initialize and start the GNSS receiver in rover mode"""
        self._hat = gnsshat.GnssHat()
        self._hat.soft_reset_hot_start()

        if not self._hat.start(create_config()):
            print("ERROR: Failed to start GNSS receiver")
            return False

        print("GNSS receiver started in RTK Rover mode.")
        return True

    def start_ntrip(self) -> bool:
        """Start the NTRIP client in a background thread."""
        print(f"Connecting to NTRIP caster: "
              f"{NTRIP_CASTER_IP}:{NTRIP_PORT}/{NTRIP_MOUNTPOINT}")

        self._ntrip_client = GNSSNTRIPClient()

        streaming = self._ntrip_client.run(
            server=NTRIP_CASTER_IP,
            port=NTRIP_PORT,
            https=int(NTRIP_HTTPS),
            mountpoint=NTRIP_MOUNTPOINT,
            datatype="RTCM",
            version=NTRIP_VERSION,
            ntripuser=NTRIP_USER,
            ntrippassword=NTRIP_PASSWORD,
            ggainterval=GGA_INTERVAL,
            ggamode=GGA_MODE,
            reflat=REFERENCE_LAT,
            reflon=REFERENCE_LON,
            refalt=REFERENCE_ALT,
            refsep=REFERENCE_SEP,
            output=self._rtcm_queue,
            stopevent=self._stop_event,
        )

        if not streaming:
            print("ERROR: NTRIP connection failed")
            status = self._ntrip_client.status
            if status:
                print(f"  HTTP status: {status.get('code', '?')} "
                      f"{status.get('description', '')}")
            return False

        print("NTRIP client connected - streaming RTCM3 corrections.\n")
        return True

    def _apply_corrections_loop(self):
        """
        Background thread: reads RTCM3 frames from the queue
        and applies them to the GNSS receiver in batches.
        """
        while not self._stop_event.is_set():
            frames = []
            try:
                raw, _parsed = self._rtcm_queue.get(timeout=1.0)
                frames.append(raw)
            except Empty:
                continue

            # Drain any additional queued frames into the same batch
            while not self._rtcm_queue.empty():
                try:
                    raw, _parsed = self._rtcm_queue.get_nowait()
                    frames.append(raw)
                except Empty:
                    break

            if not frames:
                continue

            self._hat.rtk_apply_corrections(frames)

    def run(self):
        """Main loop — prints navigation status at each epoch"""
        if not self.start_gnss():
            return -1
        if not self.start_ntrip():
            return -1

        # Start correction-application thread
        corrections_thread = Thread(
            target=self._apply_corrections_loop,
            daemon=True
        )
        corrections_thread.start()

        print("Monitoring RTK fix quality (Ctrl+C to stop)...\n")

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

                print(f"\033[1m[{gnsshat.utc_time_iso8601(nav.pvt)}]\033[0m "
                      f"Fix: {fix_quality.name} ({fix_type.name})  "
                      f"Pos: {lat:.8f}, {lon:.8f}, {alt:.2f} m  "
                      f"Acc: H={hacc:.3f} m, V={vacc:.3f} m")

        except KeyboardInterrupt:
            print("\nStopping RTK Rover...")
        except Exception as e:
            print(f"Error: {e}")
            return -1
        finally:
            self.stop()

        print("Program finished.")
        return 0

    def stop(self):
        """Stop NTRIP client and clean up."""
        self._stop_event.set()
        if self._ntrip_client is not None:
            self._ntrip_client.stop()


def main():
    rover = NtripRover()

    def signal_handler(_sig, _frame):
        rover.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    return rover.run()


if __name__ == "__main__":
    exit(main())
