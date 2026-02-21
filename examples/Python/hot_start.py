# Jimmy Paputto 2025

import time
import sys
import os

from jimmypaputto import gnsshat


def create_default_config():
    """Create default configuration for the GNSS HAT."""
    return {
        'measurement_rate_hz': 10,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'pulse_when_no_fix': None
        },
        'geofencing': None
    }


def wait_for_fix(gnss_hat, timeout_seconds=300):
    """Wait for GPS fix to be acquired."""
    start_time = time.time()

    while time.time() - start_time < timeout_seconds:
        try:
            nav = gnss_hat.wait_and_get_fresh_navigation()

            if gnsshat.FixStatus(nav.pvt.fix_status) == gnsshat.FixStatus.ACTIVE:
                return True

        except Exception as e:
            print(f"Error getting navigation: {e}")
            time.sleep(0.1)

    return False


def main():
    print("This example compares cold start vs hot start performance")

    gnss_config = create_default_config()

    with gnsshat.GnssHat() as hat:
        print("Starting GNSS HAT...")
        
        if not hat.start(gnss_config):
            print("Failed to start GNSS HAT")
            return 1

        print("GNSS HAT started successfully!")
        
        print("\n--- COLD START TEST ---")
        print("Performing hard reset (cold start)...")
        hat.hard_reset_cold_start()
        
        print("Waiting for GPS fix after cold start...")
        cold_start_time = time.time()
        
        if wait_for_fix(hat):
            cold_time_ms = int((time.time() - cold_start_time) * 1000)
            print(f"Cold start took {cold_time_ms} ms")
        else:
            print("Cold start timed out - no fix acquired")
            return 1
        
        print(
            "\nWaiting 40 seconds to collect satellite data for hot start..."
        )
        time.sleep(40)
        
        print("\n--- HOT START TEST ---")
        print("Performing soft reset (hot start)...")
        hat.soft_reset_hot_start()
        
        # Give a small delay after reset
        time.sleep(0.15)
        
        print("Waiting for GPS fix after hot start...")
        hot_start_time = time.time()

        if wait_for_fix(hat):
            hot_time_ms = int((time.time() - hot_start_time) * 1000)
            print(f"Hot start took {hot_time_ms} ms")
        else:
            print("Hot start timed out - no fix acquired")
            return 1

        print(f"\n--- RESULTS ---")
        print(f"Cold start: {cold_time_ms} ms")
        print(f"Hot start:  {hot_time_ms} ms")

        if cold_time_ms > hot_time_ms:
            improvement = ((cold_time_ms - hot_time_ms) / cold_time_ms) * 100
            print(f"Hot start was {improvement:.1f}% faster!")
        else:
            print("Hot start was not faster")

    print("\nExample completed successfully")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)
