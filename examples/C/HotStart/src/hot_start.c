/*
 * Jimmy Paputto 2025
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <jimmypaputto/GnssHat.h>


jp_gnss_gnss_config_t create_config()
{
    jp_gnss_gnss_config_t config;
    jp_gnss_gnss_config_init(&config);

    config.measurement_rate_hz = 10;
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

static int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(void)
{
    jp_gnss_hat_t* gnss = jp_gnss_hat_create();
    if (!gnss)
    {
        printf("Error: Failed to create GNSS instance\r\n");
        return -1;
    }

    jp_gnss_gnss_config_t config = create_config();
    jp_gnss_hat_hard_reset_cold_start(gnss);

    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Startup failed, exit\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("Startup done, ublox configured\r\n");

    /* --- Cold start test --- */
    jp_gnss_hat_hard_reset_cold_start(gnss);
    int64_t start = get_time_ms();

    jp_gnss_navigation_t navigation;
    while (true)
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
            continue;

        if (navigation.pvt.fix_status == JP_GNSS_FIX_STATUS_ACTIVE)
            break;
    }

    int64_t time2fix = get_time_ms() - start;
    printf("Cold start took %lld ms\r\n", (long long)time2fix);

    /* --- Wait for satellite data collection --- */
    printf("Wait 40s to collect data for hot start\r\n");
    sleep(40);
    printf("Performing hot start\r\n");

    /* --- Hot start test --- */
    jp_gnss_hat_soft_reset_hot_start(gnss);
    start = get_time_ms();
    usleep(150000);

    while (true)
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
            continue;

        if (navigation.pvt.fix_status == JP_GNSS_FIX_STATUS_ACTIVE)
            break;
    }

    int64_t hot_time = get_time_ms() - start;
    printf("Hot start took %lld ms\r\n", (long long)hot_time);

    jp_gnss_hat_destroy(gnss);

    return 0;
}
