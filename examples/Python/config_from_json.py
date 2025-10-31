# Jimmy Paputto 2025

import json
import sys
from typing import Dict, Any, Optional

from jimmypaputto import gnsshat


def load_config_from_json(json_path: str) -> Optional[Dict[str, Any]]:
    """Load configuration from JSON file."""
    try:
        with open(json_path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: Config file '{json_path}' not found")
        return None
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{json_path}': {e}")
        return None
    except Exception as e:
        print(f"Error reading '{json_path}': {e}")
        return None


def print_config_summary(config: Dict[str, Any]) -> None:
    """Print a summary of the loaded configuration."""
    print("Configuration Summary:")
    print(f"  Measurement Rate: {config.get('measurement_rate_hz', 'N/A')} Hz")
    print(f"  Dynamic Model: {config.get('dynamic_model', 'N/A')}")

    geofencing = config.get('geofencing')
    if geofencing:
        fence_count = len(geofencing.get('geofences', []))
        print(f"  Geofencing: Enabled ({fence_count} fence(s))")
        for i, fence in enumerate(geofencing.get('geofences', [])):
            lat = fence.get('latitude', 0)
            lon = fence.get('longitude', 0)
            radius = fence.get('radius_m', 0)
            print(
                f"    - Fence {i+1}: {lat:.6f}, {lon:.6f} "
                f"(radius: {radius:.2f}m)"
            )
    else:
        print("  Geofencing: Disabled")

    timepulse = config.get('timepulse_pin_config')
    if timepulse and timepulse.get('active'):
        freq = timepulse.get('fixed_pulse', {}).get('frequency', 1)
        width = timepulse.get('fixed_pulse', {}).get('pulse_width', 0.1)
        print(f"  Timepulse: Enabled ({freq} Hz, {width:.1f}s width)")
    else:
        print("  Timepulse: Disabled")
    print()


def main():
    if len(sys.argv) != 2:
        print("Usage: python config_from_json.py <config.json>")
        print("\nExample config files:")
        print("  res/config.json")
        print("  res/config_no_geofencing.json")
        print("  res/config_no_timepulse.json")
        return 1

    config_path = sys.argv[1]
    print(f"Loading configuration from: {config_path}")

    config = load_config_from_json(config_path)
    if config is None:
        return 1

    print_config_summary(config)

    print("Initializing GNSS HAT...")
    hat = gnsshat.GnssHat()

    print("Performing cold start...")
    hat.hard_reset_cold_start()

    print("Starting GNSS HAT with loaded configuration...")

    try:
        if not hat.start(config):
            print("Failed to start GNSS HAT")
            return 1
    except Exception as e:
        print(f"Failed to start GNSS HAT: {e}")
        return 1

    print("GNSS HAT started successfully!")
    print("Configuration loaded and applied successfully!")
    print("Your JSON configuration is working properly.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
