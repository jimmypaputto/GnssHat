/*
 * Jimmy Paputto 2025
 */

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

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

void print_navigation_data(const jp_gnss_navigation_t* nav)
{
    printf("GNSS Navigation Data:\r\n");
    printf("\tFix Quality: %s\r\n",
        jp_gnss_fix_quality_to_string(nav->pvt.fix_quality));
    printf("\tFix Status: %s\r\n",
        jp_gnss_fix_status_to_string(nav->pvt.fix_status));
    printf("\tFix Type: %s\r\n",
        jp_gnss_fix_type_to_string(nav->pvt.fix_type));
    printf("\tVisible Satellites: %d\n", nav->pvt.visible_satellites);
    printf("\tLatitude: %.6f°\n", nav->pvt.latitude);
    printf("\tLongitude: %.6f°\n", nav->pvt.longitude);
    printf("\tAltitude: %.2f m\n", nav->pvt.altitude);
    printf("\tTime: %s\n", jp_gnss_utc_time_iso8601(&nav->pvt));
    printf("\tTime accuracy: %d nanoseconds\n", nav->pvt.utc.accuracy);
    printf("\tDate valid: %s\n", nav->pvt.date.valid ? "true" : "false");
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

    jp_gnss_hat_soft_reset_hot_start(gnss);

    jp_gnss_gnss_config_t config = create_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Failed to start GNSS\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
        {
            print_navigation_data(&navigation);
        }
        else
        {
            printf("Error: Failed to get navigation data\n");
        }
    }

    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
