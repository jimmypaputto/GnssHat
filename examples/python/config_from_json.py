# Jimmy Paputto 2025

import json
import sys
from typing import Dict, Any, Optional

from jimmypaputto import gnsshat


# Maps JSON key names to gnsshat enum classes.
# When a key's value is a string, it is resolved via the enum.
_ENUM_KEYS: Dict[str, Any] = {
    'dynamic_model':  gnsshat.DynamicModel,
    'mode':           gnsshat.RtkMode,
    'base_mode':      gnsshat.BaseMode,
    'position_type':  gnsshat.FixedPositionType,
    'polarity':       gnsshat.TimepulsePolarity,
}


def _enum_name(enum_cls: Any, value: Any) -> str:
    """Return the member name for *value* in *enum_cls*, or str(value)."""
    for member in dir(enum_cls):
        if not member.startswith('_') and getattr(enum_cls, member) == value:
            return member
    return str(value)


def _resolve_enums(obj: Any, key: str = '') -> Any:
    """Recursively resolve string enum values to their integer values."""
    if isinstance(obj, dict):
        return {k: _resolve_enums(v, k) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_resolve_enums(v, key) for v in obj]
    if isinstance(obj, str) and key in _ENUM_KEYS:
        enum_cls = _ENUM_KEYS[key]
        name = obj.upper()
        for member in dir(enum_cls):
            if not member.startswith('_') and member.upper() == name:
                return getattr(enum_cls, member)
        raise ValueError(
            f"Unknown enum value '{obj}' for key '{key}'. "
            f"Valid: {[m for m in dir(enum_cls) if not m.startswith('_')]}"
        )
    return obj


def load_config_from_json(json_path: str) -> Optional[Dict[str, Any]]:
    """Load configuration from JSON file.

    Enum fields (dynamic_model, mode, base_mode, position_type,
    polarity) accept either integer values or string names, e.g.
    ``"dynamic_model": "STATIONARY"`` is equivalent to
    ``"dynamic_model": 2``.
    """
    try:
        with open(json_path, 'r') as f:
            config = json.load(f)
        return _resolve_enums(config)
    except FileNotFoundError:
        print(f"Error: Config file '{json_path}' not found")
        return None
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{json_path}': {e}")
        return None
    except ValueError as e:
        print(f"Error: {e}")
        return None
    except Exception as e:
        print(f"Error reading '{json_path}': {e}")
        return None


def _print_base_config(base: Dict[str, Any], prefix: str) -> None:
    """Print survey-in or fixed-position base config."""
    base_mode = _enum_name(gnsshat.BaseMode, base.get('base_mode'))
    print(f"    {prefix} Mode: {base_mode}")
    survey_in = base.get('survey_in')
    if survey_in:
        obs = survey_in.get('minimum_observation_time_s', 0)
        acc = survey_in.get('required_position_accuracy_m', 0)
        print(f"    Survey-In: {obs}s, accuracy {acc:.1f}m")
    fixed = base.get('fixed_position')
    if fixed:
        pt = _enum_name(gnsshat.FixedPositionType, fixed.get('position_type'))
        acc = fixed.get('position_accuracy_m', 0)
        print(f"    Fixed Position ({pt}), accuracy {acc:.1f}m")
        lla = fixed.get('lla')
        if lla:
            print(
                f"    LLA: {lla.get('latitude_deg', 0):.6f}, "
                f"{lla.get('longitude_deg', 0):.6f}, "
                f"{lla.get('height_m', 0):.1f}m"
            )
        ecef = fixed.get('ecef')
        if ecef:
            print(
                f"    ECEF: {ecef.get('x_m', 0):.3f}, "
                f"{ecef.get('y_m', 0):.3f}, "
                f"{ecef.get('z_m', 0):.3f}"
            )


def print_config_summary(config: Dict[str, Any]) -> None:
    """Print a summary of the loaded configuration."""
    print("Configuration Summary:")
    print(f"  Measurement Rate: {config.get('measurement_rate_hz', 'N/A')} Hz")
    dm = config.get('dynamic_model', 'N/A')
    print(f"  Dynamic Model: {_enum_name(gnsshat.DynamicModel, dm)}")

    geofencing = config.get('geofencing')
    if geofencing:
        fence_count = len(geofencing.get('geofences', []))
        print(f"  Geofencing: Enabled ({fence_count} fence(s))")
        for i, fence in enumerate(geofencing.get('geofences', [])):
            lat = fence.get('lat', 0)
            lon = fence.get('lon', 0)
            radius = fence.get('radius', 0)
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

    rtk = config.get('rtk')
    if rtk:
        mode = _enum_name(gnsshat.RtkMode, rtk.get('mode'))
        print(f"  RTK: {mode}")
        base = rtk.get('base')
        if base:
            _print_base_config(base, prefix='RTK Base')
    else:
        print("  RTK: Disabled")

    timing = config.get('timing')
    if timing:
        print("  Timing: Enabled")
        time_base = timing.get('time_base')
        if time_base:
            _print_base_config(time_base, prefix='Time Base')
    else:
        print("  Timing: Disabled")
    print()


def main():
    if len(sys.argv) != 2:
        print("Usage: python config_from_json.py <config.json>")
        print("\nExample config files:")
        print("  res/config.json")
        print("  res/config_no_geofencing.json")
        print("  res/config_no_timepulse.json")
        print("  res/config_rtk_rover.json")
        print("  res/config_rtk_base.json")
        print("  res/config_timing.json")
        return 1

    config_path = sys.argv[1]
    print(f"Loading configuration from: {config_path}")

    config = load_config_from_json(config_path)
    if config is None:
        return 1

    print_config_summary(config)

    print("Initializing GNSS HAT...")
    hat = gnsshat.GnssHat()

    print("Performing hot start...")
    hat.soft_reset_hot_start()

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
