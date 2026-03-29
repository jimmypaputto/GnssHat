# Jimmy Paputto 2026

import signal
import threading
import time

from jimmypaputto import gnsshat


running = True


def signal_handler(sig, frame):
    global running
    print("\n\nReceived SIGINT, shutting down gracefully...")
    running = False


def create_config() -> dict:
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'timing': {
            'enable_time_mark': True
        }
    }


def toggle_thread_func(hat):
    while running:
        hat.trigger_time_mark(gnsshat.TimeMarkTriggerEdge.TOGGLE)
        print("[EXTINT] toggled")

        for _ in range(50):
            if not running:
                return
            time.sleep(0.1)


def time_mark_thread_func(hat):
    while running:
        try:
            tm = hat.wait_and_get_fresh_time_mark()
        except RuntimeError:
            break

        if not running:
            return

        print(tm)


def main():
    signal.signal(signal.SIGINT, signal_handler)

    with gnsshat.GnssHat() as hat:
        hat.soft_reset_hot_start()

        if not hat.start(create_config()):
            print("Startup failed, exit")
            return -1

        print("Startup done, ublox configured")

        hat.enable_time_mark_trigger()
        print("TimeMark trigger enabled, toggling EXTINT every 5s\n")

        toggle = threading.Thread(target=toggle_thread_func, args=(hat,), daemon=True)
        tm_reader = threading.Thread(target=time_mark_thread_func, args=(hat,), daemon=True)

        toggle.start()
        tm_reader.start()

        while running:
            time.sleep(0.1)

        hat.disable_time_mark_trigger()

    toggle.join()
    tm_reader.join()

    print("Exiting...")
    return 0


if __name__ == "__main__":
    exit(main())
