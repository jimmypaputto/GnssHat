# Jimmy Paputto 2025

from jimmypaputto import gnsshat


def create_default_config() -> dict:
    """Create configuration optimized for jamming detection"""
    return {
        'measurement_rate_hz': 5,
        'dynamic_model': gnsshat.DYNAMIC_MODEL_STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'pulse_when_no_fix': None,
            'polarity': gnsshat.TIMEPULSE_POLARITY_RISING_EDGE
        },
        'geofencing': None  # No geofencing for jamming detection
    }


def rf_band_to_str(band_id: int) -> str:
    """Convert RF band ID to human readable string"""
    return gnsshat.rf_band_to_string(band_id)


def jamming_state_to_str(state: int) -> str:
    """Convert jamming state to human readable string"""
    return gnsshat.jamming_state_to_string(state)


def antenna_status_to_str(status: int) -> str:
    """Convert antenna status to human readable string"""
    return gnsshat.antenna_status_to_string(status)


def antenna_power_to_str(power: int) -> str:
    """Convert antenna power to human readable string"""
    return gnsshat.antenna_power_to_string(power)


def print_rf_block(rf_block):
    """Print detailed RF block information"""
    print(f"RF Block {rf_block.id} ({rf_band_to_str(rf_block.id)} band):")
    print(f"  Noise per ms: {rf_block.noise_per_ms}")
    print(f"  AGC monitor, percentage of max gain: {rf_block.agc_monitor:.2f}%")
    print(f"  Antenna status: {antenna_status_to_str(rf_block.antenna_status)}")
    print(f"  Antenna power: {antenna_power_to_str(rf_block.antenna_power)}")
    print(f"  Jamming state: {jamming_state_to_str(rf_block.jamming_state)}")
    print(f"  CW interference suppression level: "
          f"{rf_block.cw_interference_suppression_level:.2f}%")
    print(f"  POST status: 0x{rf_block.post_status:08X}")
    print(f"  I/Q offsets: I={rf_block.ofs_i}, Q={rf_block.ofs_q}")
    print(f"  I/Q magnitudes: I={rf_block.mag_i}, Q={rf_block.mag_q}")


def analyze_rf_block(rf_block):
    JAMMING_THRESHOLD = 40.00
    if rf_block.cw_interference_suppression_level < JAMMING_THRESHOLD:
        print(f"  CW interference suppression level is below "
              f"{JAMMING_THRESHOLD:.2f}%, no jamming detected")
    else:
        print(f"  CW interference suppression level is above "
              f"{JAMMING_THRESHOLD:.2f}%, jamming detected")


def main():
    ubx_hat = gnsshat.GnssHat()
    ubx_hat.soft_reset_hot_start()

    is_startup_done = ubx_hat.start(create_default_config())
    if not is_startup_done:
        print("Startup failed, exit")
        return -1

    print("Startup done, ublox configured")
    print("Monitoring for jamming and interference...")
    print("Press Ctrl+C to stop\n")

    try:
        while True:
            rf_blocks = ubx_hat.wait_and_get_fresh_navigation().rf_blocks
            for rf_block in rf_blocks:
                print_rf_block(rf_block)
                analyze_rf_block(rf_block)
                print()

    except KeyboardInterrupt:
        print("\nStopping jamming detection...")
    except Exception as e:
        print(f"Error: {e}")
        return -1
    
    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
