# Jimmy Paputto 2025

#
# RTK Rover example (Python)
#
# Demonstrates configuring the GNSS module as an RTK Rover.
# The rover receives RTCM3 correction data from a base station
# to achieve centimeter-level positioning accuracy.
#

from jimmypaputto import gnsshat


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


def main():
    hat = gnsshat.GnssHat()
    hat.hard_reset_cold_start()

    is_startup_done = hat.start(create_config())
    if not is_startup_done:
        print("Failed to start GNSS")
        return -1

    print("GNSS started as RTK Rover. Monitoring fix quality...\n")

    try:
        while True:
            nav = hat.wait_and_get_fresh_navigation()
            fix_quality = gnsshat.FixQuality(nav.pvt.fix_quality)
            fix_type = gnsshat.FixType(nav.pvt.fix_type)
            print(f"Fix Quality: {fix_quality.name}, "
                  f"Fix Type: {fix_type.name}")

    except KeyboardInterrupt:
        print("\nStopping RTK Rover monitoring...")
    except Exception as e:
        print(f"Error: {e}")
        return -1

    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
