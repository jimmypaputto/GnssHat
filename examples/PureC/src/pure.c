/*
 * Jimmy Paputto 2025
 */

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
    printf("Navigation:\r\n");
    printf("  Fix Quality: %d\n", nav->pvt.fix_quality);
    printf("  Fix Status: %d\n", nav->pvt.fix_status);
    printf("  Fix Type: %d\n", nav->pvt.fix_type);
    
    if (nav->pvt.latitude != 0.0 || nav->pvt.longitude != 0.0)
    {
        printf("  Latitude: %.8f°\n", nav->pvt.latitude);
        printf("  Longitude: %.8f°\n", nav->pvt.longitude);
        printf("  Altitude: %.2f m\n", nav->pvt.altitude);
        printf("  Visible Satellites: %d\n", nav->pvt.visible_satellites);
        printf("  Horizontal Accuracy: %.2f m\n", nav->pvt.horizontal_accuracy);
    }
    else
    {
        printf("  No valid position data yet (all zeros)\n");
    }
    
    if (nav->pvt.utc.valid)
    {
        printf(
            "  UTC Time: %02d:%02d:%02d\n",
            nav->pvt.utc.hh,
            nav->pvt.utc.mm,
            nav->pvt.utc.ss
        );
    }
    else
    {
        printf("  UTC Time: Invalid\n");
    }
}

jp_gnss_gnss_config_t create_config()
{
    jp_gnss_gnss_config_t config;

    config.measurement_rate_hz = 1;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_PORTABLE;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = 1;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f; // 100ms pulse
    config.timepulse_pin_config.has_pulse_when_no_fix = false;
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    // No geofencing for this example
    config.has_geofencing = false;
    config.geofencing.geofence_count = 0;

    return config;
}

int main()
{
    signal(SIGINT, signal_handler);

    jp_gnss_hat_t* gnss = jp_gnss_hat_create();
    if (!gnss)
    {
        printf("Error: Failed to create GNSS instance\r\n");
        return -1;
    }
    jp_gnss_hat_hard_reset_cold_start(gnss);

    jp_gnss_gnss_config_t config = create_config();
    printf("Configuration prepared successfully\r\n");

    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Error: Failed to start GNSS\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }
    
    printf("Startup done\n\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (jp_gnss_hat_get_navigation(gnss, &navigation))
        {
            print_navigation_data(&navigation);
        }
        else
        {
            printf("Error: Failed to get navigation data\n");
        }

        sleep(1);
    }

    printf("Destroying GNSS instance...\n");
    fflush(stdout);

    jp_gnss_hat_destroy(gnss);

    printf("GNSS instance destroyed\n");
    printf("Application terminated gracefully\n");

    return 0;
}
