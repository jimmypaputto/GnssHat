/*
 * Jimmy Paputto 2025
 */

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

const char* confidence_level_to_str(uint8_t level)
{
    switch (level)
    {
        case 0: return "No confidence required";
        case 1: return "68%";
        case 2: return "95%";
        case 3: return "99.7%";
        case 4: return "99.99%";
        case 5: return "99.9999%";
    }
    return "Fatal error";
}

void print_geofencing(const jp_gnss_geofencing_t* geofencing)
{
    printf("Geofencing:\r\n");
    printf("  Configuration:\r\n");
    printf("    Confidence level: %s\r\n",
        confidence_level_to_str(geofencing->cfg.confidence_level));
    printf("    Geofences:\r\n");

    for (uint8_t i = 0; i < geofencing->cfg.geofence_count; ++i)
    {
        printf("      Latitude: %f, Longitude: %f, Radius: %.2f m\r\n",
            geofencing->cfg.geofences[i].lat,
            geofencing->cfg.geofences[i].lon,
            geofencing->cfg.geofences[i].radius);
    }

    printf("  Navigation:\r\n");
    printf("    Geofencing status: %s\r\n",
        jp_gnss_geofencing_status_to_string(
            geofencing->nav.geofencing_status));
    printf("    Number of geofences: %d\r\n",
        geofencing->nav.number_of_geofences);
    printf("    Combined state: %s\r\n",
        jp_gnss_geofence_status_to_string(geofencing->nav.combined_state));

    for (uint8_t i = 0; i < geofencing->nav.number_of_geofences; ++i)
    {
        printf("      Geofence no %d: %s\r\n",
            i + 1,
            jp_gnss_geofence_status_to_string(
                geofencing->nav.geofences_status[i]));
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

    config.has_geofencing = true;
    config.geofencing.confidence_level = 3;
    config.geofencing.has_pin_polarity = true;
    config.geofencing.pin_polarity = JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_OUTSIDE;

    jp_gnss_geofence_t rome = { .lat = 41.902205f, .lon = 12.453920f,
        .radius = 2005.0f };
    jp_gnss_geofence_t warsaw = { .lat = 52.257211f, .lon = 20.311759f,
        .radius = 1810.0f };

    jp_gnss_gnss_config_add_geofence(&config, rome);
    jp_gnss_gnss_config_add_geofence(&config, warsaw);

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

    jp_gnss_hat_hard_reset_cold_start(gnss);

    jp_gnss_gnss_config_t config = create_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Startup failed, exit\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("Startup done, ublox configured\r\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
        {
            print_geofencing(&navigation.geofencing);
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
