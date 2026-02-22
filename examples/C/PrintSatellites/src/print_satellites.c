/*
 * Jimmy Paputto 2026
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

void print_satellites(const jp_gnss_navigation_t* nav)
{
    printf("=== Satellite Information ===\r\n");
    printf("Total satellites: %d\r\n", nav->num_satellites);
    printf("%-10s %-6s %-8s %-12s %-10s %-14s %-10s %-8s\r\n",
           "System", "SV ID", "C/N0", "Elevation", "Azimuth", "Quality",
           "Used", "Health");
    printf("---------- ------ -------- ------------ ---------- -------------- "
           "---------- --------\r\n");

    uint8_t used_count = 0;
    for (int i = 0; i < nav->num_satellites; i++)
    {
        const jp_gnss_satellite_info_t* sat = &nav->satellites[i];

        if (sat->used_in_fix)
            used_count++;

        printf(
            "%-10s %-6d %-5d dB  %5d\xC2\xB0       %5d\xC2\xB0"
            "     %-14s %-10s %-8s\r\n",
            jp_gnss_gnss_id_to_string(sat->gnss_id),
            sat->sv_id,
            sat->cno,
            sat->elevation,
            sat->azimuth,
            jp_gnss_sv_quality_to_string(sat->quality),
            sat->used_in_fix ? "Yes" : "No",
            sat->healthy ? "OK" : "Bad"
        );
    }

    printf("---------- ------ -------- ------------ ---------- -------------- "
           "---------- --------\r\n");
    printf("Satellites used in fix: %d / %d\r\n",
        used_count, nav->num_satellites);

    /* Feature availability counters */
    uint8_t eph_count = 0;
    uint8_t alm_count = 0;
    uint8_t diff_count = 0;
    for (int i = 0; i < nav->num_satellites; i++)
    {
        if (nav->satellites[i].eph_avail) eph_count++;
        if (nav->satellites[i].alm_avail) alm_count++;
        if (nav->satellites[i].diff_corr) diff_count++;
    }
    printf("Ephemeris: %d  Almanac: %d  DGPS: %d\r\n\n",
        eph_count, alm_count, diff_count);
}

jp_gnss_gnss_config_t create_config(void)
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

    printf("GNSS started successfully. Monitoring satellite data...\r\n\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
        {
            print_satellites(&navigation);
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
