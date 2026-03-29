/*
 * Jimmy Paputto 2026
 *
 * Time Base example for L1/L5 GNSS TIME HAT (NEO-F10T)
 *
 * Demonstrates configuring the timing module in "time base" mode
 * using the C API. Shows three configuration modes:
 *   - Survey-In: The module determines its position automatically
 *   - Fixed Position (LLA): Provide known lat/lon/height
 *   - Fixed Position (ECEF): Provide known ECEF coordinates
 *
 * Once configured, the timing module achieves better time precision
 * by entering a "time fix" mode (TimeOnlyFix).
 */

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <jimmypaputto/GnssHat.h>


static atomic_bool running = true;

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\n\nReceived SIGINT, shutting down gracefully...\n");
        atomic_store(&running, false);
    }
}

jp_gnss_gnss_config_t create_survey_in_config()
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 1;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_STATIONARY;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = 1;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config.timepulse_pin_config.has_pulse_when_no_fix = false;
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    config.has_timing = true;
    config.timing.has_time_base = true;
    config.timing.time_base.base_mode = JP_GNSS_BASE_MODE_SURVEY_IN;
    config.timing.time_base.survey_in.minimum_observation_time_s = 120;
    config.timing.time_base.survey_in.required_position_accuracy_m = 50.0;

    return config;
}

jp_gnss_gnss_config_t create_fixed_position_lla_config()
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 1;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_STATIONARY;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = 1;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config.timepulse_pin_config.has_pulse_when_no_fix = false;
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    config.has_timing = true;
    config.timing.has_time_base = true;
    config.timing.time_base.base_mode = JP_GNSS_BASE_MODE_FIXED_POSITION;
    config.timing.time_base.fixed_position.position_type = JP_GNSS_FIXED_POSITION_LLA;
    config.timing.time_base.fixed_position.lla.latitude_deg  = 52.232222222;
    config.timing.time_base.fixed_position.lla.longitude_deg = 21.008055556;
    config.timing.time_base.fixed_position.lla.height_m      = 110.0;
    config.timing.time_base.fixed_position.position_accuracy_m = 0.5;

    return config;
}

jp_gnss_gnss_config_t create_fixed_position_ecef_config()
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 1;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_STATIONARY;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = 1;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config.timepulse_pin_config.has_pulse_when_no_fix = false;
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    config.has_timing = true;
    config.timing.has_time_base = true;
    config.timing.time_base.base_mode = JP_GNSS_BASE_MODE_FIXED_POSITION;
    config.timing.time_base.fixed_position.position_type = JP_GNSS_FIXED_POSITION_ECEF;
    config.timing.time_base.fixed_position.ecef.x_m = 3656215.987;
    config.timing.time_base.fixed_position.ecef.y_m = 1409547.654;
    config.timing.time_base.fixed_position.ecef.z_m = 5049982.321;
    config.timing.time_base.fixed_position.position_accuracy_m = 0.5;

    return config;
}

int main(void)
{
    signal(SIGINT, signal_handler);

    jp_gnss_hat_t* gnss = jp_gnss_hat_create();
    if (!gnss)
    {
        printf("Error: Failed to create GNSS instance\r\n");
        return -1;
    }

    jp_gnss_hat_soft_reset_hot_start(gnss);

    /* Use Survey-In mode by default.
     * Alternatively try: create_fixed_position_lla_config()
     *                or: create_fixed_position_ecef_config()  */
    jp_gnss_gnss_config_t config = create_survey_in_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Failed to start GNSS\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("GNSS started with time base configuration (Survey-In mode).\r\n");
    printf("Waiting for TimeOnlyFix...\r\n\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
            continue;

        const char* time_str = jp_gnss_utc_time_iso8601(&navigation.pvt);
        const char* fix_type_str =
            jp_gnss_fix_type_to_string(navigation.pvt.fix_type);

        printf("[%s] Fix: %s  tAcc: %d ns  Lat: %.7f  Lon: %.7f  Alt: %.2f m\r\n",
            time_str,
            fix_type_str,
            navigation.pvt.utc.accuracy,
            navigation.pvt.latitude,
            navigation.pvt.longitude,
            navigation.pvt.altitude_msl);
    }

    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
