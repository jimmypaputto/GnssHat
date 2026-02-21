# Jimmy Paputto 2025

from jimmypaputto import gnsshat

def create_config() -> dict:
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DYNAMIC_MODEL_PORTABLE,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TIMEPULSE_POLARITY_RISING_EDGE
        },
        'geofencing': None
    }

hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
isStartupDone = hat.start(create_config())
if isStartupDone:
    print("GNSS HAT started successfully!")
else:
    print("Failed to start GNSS HAT, exiting...")
    exit(1)

while True:
    try:
        nav = hat.wait_and_get_fresh_navigation()
        print("\033[2J\033[H")  # clear terminal
        print(nav)

    except KeyboardInterrupt:
        print("Exiting...")
        break
    except Exception as e:
        print(f"Error: {e}")
        break
