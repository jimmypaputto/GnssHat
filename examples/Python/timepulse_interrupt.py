# Jimmy Paputto 2025

from jimmypaputto import gnsshat


PULSE_RATE_HZ = 5


def create_default_config() -> dict:
    """Create configuration with timepulse enabled at PULSE_RATE_HZ"""
    pulse = {
        'frequency': PULSE_RATE_HZ,
        'pulse_width': 0.1
    }
    return {
        'measurement_rate_hz': PULSE_RATE_HZ,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': pulse,
            'pulse_when_no_fix': pulse,
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None
    }


def main():
    hat = gnsshat.GnssHat()
    hat.hard_reset_cold_start()

    is_startup_done = hat.start(create_default_config())
    if not is_startup_done:
        print("Startup failed, exit")
        return -1

    print("Startup done, ublox configured")

    counter = 0
    try:
        while True:
            hat.timepulse()
            nav = hat.navigation()
            utc = nav.pvt.utc_time
            print(f"Timepulse: {counter}, {utc}")
            counter += 1

    except KeyboardInterrupt:
        print("\nStopping timepulse monitoring...")

    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
