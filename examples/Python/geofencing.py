# Jimmy Paputto 2025

from jimmypaputto import gnsshat


def create_default_config() -> dict:
    """
    Geofencing confidence levels map:
    0 - no confidence required
    1 - 68%
    2 - 95%
    3 - 99.7%
    4 - 99.99%
    5 - 99.9999%
    """
    geofencing_config = {
        'confidence_level': 3,  # 99.7%
        'geofences': [
            {
                'lat': 41.902205071091224,
                'lon': 12.4539203390548,
                'radius': 2005
            },
            {
                'lat': 52.257211745024186,
                'lon': 20.311759615806704,
                'radius': 1810
            }
        ]
    }

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
        'geofencing': geofencing_config
    }


def confidence_level_to_str(level: int) -> str:
    """Convert confidence level to human readable string"""
    confidence_map = {
        0: "No confidence required",
        1: "68%",
        2: "95%",
        3: "99.7%",
        4: "99.99%",
        5: "99.9999%"
    }
    return confidence_map.get(level, "Fatal error")


def geofencing_status_to_str(status: int) -> str:
    """Convert geofencing status to human readable string"""
    return gnsshat.GeofencingStatus(status).name


def geofence_status_to_str(status: int) -> str:
    """Convert geofence status to human readable string"""
    return gnsshat.GeofenceStatus(status).name


def print_geofencing(geofencing):
    """Print geofencing configuration and status"""
    print("Geofencing:")
    print("  Configuration:")

    confidence_str = confidence_level_to_str(geofencing.cfg.confidence_level)
    print(f"    Confidence level: {confidence_str}")

    print("    Geofences:")
    for geofence in geofencing.cfg.geofences:
        print(f"      Latitude: {geofence.lat:.6f}, "
              f"Longitude: {geofence.lon:.6f}, "
              f"Radius: {geofence.radius:.2f} m")

    print("  Navigation:")

    status_str = geofencing_status_to_str(geofencing.nav.status)
    print(f"    Geofencing status: {status_str}")
    print(f"    Number of geofences: {geofencing.nav.number_of_geofences}")
    
    combined_state_str = geofence_status_to_str(geofencing.nav.combined_state)
    print(f"    Combined state: {combined_state_str}")
    
    for i, fence_status in enumerate(geofencing.nav.geofences):
        status_str = geofence_status_to_str(fence_status)
        print(f"      Geofence no {i+1}: {status_str}")


def main():
    ubx_hat = gnsshat.GnssHat()
    ubx_hat.hard_reset_cold_start()

    is_startup_done = ubx_hat.start(create_default_config())
    if not is_startup_done:
        print("Startup failed, exit")
        return -1

    print("Startup done, ublox configured")

    try:
        while True:
            navigation = ubx_hat.wait_and_get_fresh_navigation()
            geofencing = navigation.geofencing
            print_geofencing(geofencing)
            print()  # Empty line for readability
            
    except KeyboardInterrupt:
        print("\nStopping geofencing monitoring...")
    except Exception as e:
        print(f"Error: {e}")
        return -1
    
    print("Program finished")
    return 0


if __name__ == "__main__":
    exit(main())
