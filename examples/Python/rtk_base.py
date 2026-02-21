# Jimmy Paputto 2025

#
# RTK Base Station example (Python)
#
# Demonstrates configuring the GNSS module as an RTK Base station.
# Shows three configuration modes:
#   - Survey-In: The module determines its position automatically
#   - Fixed Position (LLA): Provide known lat/lon/height
#   - Fixed Position (ECEF): Provide known ECEF coordinates
#
# Once configured, the base station produces RTCM3 correction
# frames that can be forwarded to an RTK rover.
#

from jimmypaputto import gnsshat


def create_survey_in_config() -> dict:
    """Create base station configuration using Survey-In mode"""
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
            'base': {
                'base_mode': gnsshat.BaseMode.SURVEY_IN,
                'survey_in': {
                    'minimum_observation_time_s': 120,
                    'required_position_accuracy_m': 50.0
                }
            }
        }
    }


def create_fixed_position_lla_config() -> dict:
    """Create base station configuration using Fixed Position (LLA)"""
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
            'base': {
                'base_mode': gnsshat.BaseMode.FIXED_POSITION,
                'fixed_position': {
                    'position_type': gnsshat.FixedPositionType.LLA,
                    'lla': {
                        'latitude_deg': 52.232222222,
                        'longitude_deg': 21.008055556,
                        'height_m': 110.0
                    },
                    'position_accuracy_m': 0.5
                }
            }
        }
    }


def create_fixed_position_ecef_config() -> dict:
    """Create base station configuration using Fixed Position (ECEF)"""
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
            'base': {
                'base_mode': gnsshat.BaseMode.FIXED_POSITION,
                'fixed_position': {
                    'position_type': gnsshat.FixedPositionType.ECEF,
                    'ecef': {
                        'x_m': 3656215.987,
                        'y_m': 1409547.654,
                        'z_m': 5049982.321
                    },
                    'position_accuracy_m': 0.5
                }
            }
        }
    }


def get_rtcm3_frame_id(frame: bytes) -> int:
    """Extract RTCM3 message ID from frame bytes"""
    if len(frame) < 6 or frame[0] != 0xD3:
        return 0
    return (frame[3] << 4) | (frame[4] >> 4)


def print_rtcm3_frame(frame: bytes, time_str: str):
    """Print RTCM3 frame details"""
    msg_id = get_rtcm3_frame_id(frame)
    hex_str = ' '.join(f'{b:02X}' for b in frame)
    print(f"[{time_str}] Frame {msg_id}: {len(frame)} bytes: {hex_str}")


def main():
    hat = gnsshat.GnssHat()
    hat.soft_reset_hot_start()

    # Use Survey-In mode by default.
    # Alternatively try: create_fixed_position_lla_config()
    #                 or: create_fixed_position_ecef_config()
    is_startup_done = hat.start(create_survey_in_config())
    if not is_startup_done:
        print("Failed to start GNSS")
        return -1

    print("GNSS started as RTK Base (Survey-In mode).")
    print("Waiting for corrections...\n")

    try:
        while True:
            nav = hat.wait_and_get_fresh_navigation()
            fix_type = gnsshat.FixType(nav.pvt.fix_type)

            if fix_type != gnsshat.FixType.TIME_ONLY_FIX:
                print(f"Fix type: {fix_type.name}, "
                      f"waiting for TIME_ONLY_FIX for RTK Base")
            else:
                print("RTK Base ready with TIME_ONLY_FIX")
                try:
                    corrections = hat.rtk_get_tiny_corrections()
                    for frame in corrections:
                        print_rtcm3_frame(frame, str(nav.pvt.utc_time))
                except RuntimeError as e:
                    print(f"RTK corrections not available: {e}")

    except KeyboardInterrupt:
        print("\nStopping RTK Base monitoring...")
    except Exception as e:
        print(f"Error: {e}")
        return -1

    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
