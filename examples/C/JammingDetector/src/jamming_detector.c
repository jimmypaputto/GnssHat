/*
 * Jimmy Paputto 2025
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

    config.measurement_rate_hz = 5;
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

void print_rf_block(const jp_gnss_rf_block_t* rf_block)
{
    printf("Band %s\r\n", jp_gnss_rf_band_to_string(rf_block->id));
    printf("    Noise per ms: %d\r\n", rf_block->noise_per_ms);
    printf("    AGC monitor, percentage of max gain: %.2f%%\r\n",
        rf_block->agc_monitor);
    printf("    Antenna status: %s\r\n",
        jp_gnss_antenna_status_to_string(rf_block->antenna_status));
    printf("    JammingState: %s\r\n",
        jp_gnss_jamming_state_to_string(rf_block->jamming_state));
    printf("    CW interference suppression level: %.2f%%\r\n",
        rf_block->cw_interference_suppression_level);
}

void analyze_rf_block(const jp_gnss_rf_block_t* rf_block)
{
    const float jamming_threshold = 40.0f;
    if (rf_block->cw_interference_suppression_level < jamming_threshold)
    {
        printf(
            "CW interference suppression level is below %.f%%, "
            "no jamming\r\n",
            jamming_threshold
        );
    }
    else
    {
        printf(
            "CW interference suppression level is above %.f%%, "
            "jamming detected\r\n",
            jamming_threshold
        );
    }
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

    jp_gnss_gnss_config_t config = create_config();
    if (!jp_gnss_hat_start(gnss, &config))
    {
        printf("Startup failed, exit\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    printf("Startup done, monitoring for jamming...\r\n");

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
        {
            printf("Error: Failed to get navigation data\n");
            continue;
        }

        for (uint8_t i = 0; i < navigation.num_rf_blocks; ++i)
        {
            print_rf_block(&navigation.rf_blocks[i]);
            analyze_rf_block(&navigation.rf_blocks[i]);
        }
    }

    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
