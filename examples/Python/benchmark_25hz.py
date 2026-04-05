# Jimmy Paputto 2026
# Benchmark: measures frames/s received via wait_and_get_fresh_navigation() at 25Hz.
# Output: one ISO 8601 UTC time per line
# Usage: sudo python3 benchmark_25hz.py > /tmp/bench_py.csv

from jimmypaputto import gnsshat

def create_benchmark_config() -> dict:
    return {
        'measurement_rate_hz': 25,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None
    }

hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
is_startup_done = hat.start(create_benchmark_config())
if not is_startup_done:
    print("Failed to start GNSS HAT, exiting...", flush=True)
    exit(1)

import sys
print("Benchmark started at 25Hz. Ctrl+C to stop.", file=sys.stderr, flush=True)

try:
    while True:
        nav = hat.wait_and_get_fresh_navigation()
        print(gnsshat.utc_time_iso8601(nav.pvt), flush=True)
except KeyboardInterrupt:
    print("Stopped.", file=sys.stderr, flush=True)
