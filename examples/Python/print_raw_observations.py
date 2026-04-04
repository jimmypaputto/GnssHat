# Jimmy Paputto 2026

from jimmypaputto import gnsshat


def create_config() -> dict:
    return {
        'measurement_rate_hz': 1,
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


def gnss_id_name(gnss_id: int) -> str:
    names = {
        gnsshat.GnssId.GPS:     "GPS",
        gnsshat.GnssId.SBAS:    "SBAS",
        gnsshat.GnssId.GALILEO: "Galileo",
        gnsshat.GnssId.BEIDOU:  "BeiDou",
        gnsshat.GnssId.IMES:    "IMES",
        gnsshat.GnssId.QZSS:    "QZSS",
        gnsshat.GnssId.GLONASS: "GLONASS",
        gnsshat.GnssId.NAVIC:   "NAVIC",
    }
    return names.get(gnss_id, "Unknown")


def gnss_signal_id_name(gnss_id: int, sig_id: int) -> str:
    signal_names = {
        gnsshat.GnssId.GPS: {
            0: "L1CA", 3: "L2CL", 4: "L2CM", 6: "L5I", 7: "L5Q"
        },
        gnsshat.GnssId.SBAS: {
            0: "L1CA"
        },
        gnsshat.GnssId.GALILEO: {
            0: "E1C", 1: "E1B", 3: "E5aI", 4: "E5aQ", 5: "E5bI", 6: "E5bQ"
        },
        gnsshat.GnssId.BEIDOU: {
            0: "B1ID1", 1: "B1ID2", 2: "B2ID1", 3: "B2ID2",
            5: "B1Cp", 6: "B1Cd", 7: "B2ap", 8: "B2ad"
        },
        gnsshat.GnssId.QZSS: {
            0: "L1CA", 1: "L1S", 4: "L2CM", 5: "L2CL", 8: "L5I", 9: "L5Q"
        },
        gnsshat.GnssId.GLONASS: {
            0: "L1OF", 2: "L2OF"
        },
        gnsshat.GnssId.NAVIC: {
            0: "L5A"
        },
    }
    return signal_names.get(gnss_id, {}).get(sig_id, "Unknown")


def print_raw_observations(nav):
    raw = nav.raw_measurements
    observations = raw.observations

    print("=== Raw Measurements (UBX-RXM-RAWX) ===")
    print(f"Receiver TOW: {raw.rcv_tow:.3f} s  Week: {raw.week}  "
          f"Leap seconds: {raw.leap_s}")
    print(f"Num meas: {raw.num_meas}  Version: {raw.version}  "
          f"Leap sec determined: {raw.leap_sec_determined}  "
          f"Clk reset: {raw.clk_reset}")
    print(f"Observations: {len(observations)}")
    print()

    print(f"{'System':<10} {'SV':<4} {'Sig':<6} {'C/N0':<6} "
          f"{'PR (m)':<16} {'CP (cyc)':<16} {'Doppler (Hz)':<14} "
          f"{'Lock':<6} {'PR✓':<4} {'CP✓':<4}")
    print("-" * 102)

    for obs in observations:
        print(f"{gnss_id_name(obs.gnss_id):<10} "
              f"{obs.sv_id:<4} "
              f"{gnss_signal_id_name(obs.gnss_id, obs.sig_id):<6} "
              f"{obs.cno:<4} dB "
              f"{obs.pr_mes:<16.3f} "
              f"{obs.cp_mes:<16.3f} "
              f"{obs.do_mes:<14.3f} "
              f"{obs.locktime:<6} "
              f"{'Y' if obs.pr_valid else 'N':<4} "
              f"{'Y' if obs.cp_valid else 'N':<4}")

    print("-" * 102)
    print()


hat = gnsshat.GnssHat()
hat.soft_reset_hot_start()
is_startup_done = hat.start(create_config())
if is_startup_done:
    print("GNSS HAT started successfully!")
else:
    print("Failed to start GNSS HAT, exiting...")
    exit(1)

while True:
    try:
        nav = hat.wait_and_get_fresh_navigation()
        print("\033[2J\033[H", end="")  # clear terminal
        print(f"Time: {gnsshat.utc_time_iso8601(nav)}")
        print()
        print_raw_observations(nav)

    except KeyboardInterrupt:
        print("Exiting...")
        break
    except Exception as e:
        print(f"Error: {e}")
        break
