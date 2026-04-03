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
    }
    return names.get(gnss_id, "Unknown")


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

    print(f"{'System':<10} {'SV':<4} {'Sig':<4} {'C/N0':<6} "
          f"{'PR (m)':<16} {'CP (cyc)':<16} {'Doppler (Hz)':<14} "
          f"{'Lock':<6} {'PR✓':<4} {'CP✓':<4}")
    print("-" * 100)

    for obs in observations:
        print(f"{gnss_id_name(obs.gnss_id):<10} "
              f"{obs.sv_id:<4} "
              f"{obs.sig_id:<4} "
              f"{obs.cno:<4} dB "
              f"{obs.pr_mes:<16.3f} "
              f"{obs.cp_mes:<16.3f} "
              f"{obs.do_mes:<14.3f} "
              f"{obs.locktime:<6} "
              f"{'Y' if obs.pr_valid else 'N':<4} "
              f"{'Y' if obs.cp_valid else 'N':<4}")

    print("-" * 100)
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
