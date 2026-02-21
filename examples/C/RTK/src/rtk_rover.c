/*
 * Jimmy Paputto 2025
 *
 * RTK Rover example (C)
 *
 * Demonstrates configuring the GNSS module as an RTK Rover using
 * the C API. The rover receives RTCM3 correction data from a base
 * station to achieve centimeter-level positioning accuracy.
 */

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
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

jp_gnss_gnss_config_t create_config()
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

    config.has_geofencing = false;

    config.has_rtk = true;
    config.rtk.mode = JP_GNSS_RTK_MODE_ROVER;
    config.rtk.has_base_config = false;

    return config;
}

int main(void)
{
    signal(SIGINT, signal_handler);

    jp_gnss_hat_t* gnss = jp_gnss_hat_create();
    if (!gnss)
    {
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    jp_gnss_hat_hard_reset_cold_start(gnss);

    jp_gnss_gnss_config_t config = create_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Failed to start GNSS\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("GNSS started as RTK Rover. Monitoring fix quality...\r\n\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
            continue;

        printf("[%s] Fix Quality: %s, Fix Type: %s\r\n",
            jp_gnss_utc_time_iso8601(&navigation.pvt),
            jp_gnss_fix_quality_to_string(navigation.pvt.fix_quality),
            jp_gnss_fix_type_to_string(navigation.pvt.fix_type)
        );
    }

    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
