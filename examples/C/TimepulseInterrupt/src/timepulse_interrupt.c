/*
 * Jimmy Paputto 2025
 */

#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <jimmypaputto/GnssHat.h>


#define PULSE_RATE_HZ 5

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

    config.measurement_rate_hz = PULSE_RATE_HZ;
    config.dynamic_model = JP_GNSS_DYNAMIC_MODEL_STATIONARY;

    config.timepulse_pin_config.active = true;
    config.timepulse_pin_config.fixed_pulse.frequency = PULSE_RATE_HZ;
    config.timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config.timepulse_pin_config.has_pulse_when_no_fix = true;
    config.timepulse_pin_config.pulse_when_no_fix.frequency = PULSE_RATE_HZ;
    config.timepulse_pin_config.pulse_when_no_fix.pulse_width = 0.1f;
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
    uint32_t counter = 0;

    while (atomic_load(&running))
    {
        jp_gnss_hat_timepulse(gnss);

        if (jp_gnss_hat_get_navigation(gnss, &navigation))
        {
            printf("Timepulse: %u, %02d:%02d:%02d\r\n",
                counter++,
                navigation.pvt.utc.hh,
                navigation.pvt.utc.mm,
                navigation.pvt.utc.ss);
        }
    }

    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
