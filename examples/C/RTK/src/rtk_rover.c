/*
 * Jimmy Paputto 2025
 *
 * RTK Rover example (C)
 *
 * Demonstrates configuring the GNSS module as an RTK Rover.
 * The corrections thread shows where RTCM3 data from an NTRIP
 * caster should be applied to the receiver.
 *
 * NOTE: A full NTRIP client implementation is available in the
 * Python example (examples/Python/rtk_rover.py) using pygnssutils.
 * A native C++ NTRIP client is under development and will be
 * added here in a future release.
 */

#include <pthread.h>
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


/*
 * Correction application thread.
 *
 * In a real setup this thread would:
 *   1. Connect to an NTRIP caster (host, port, mountpoint, credentials)
 *   2. Receive a stream of RTCM3 frames
 *   3. Forward them to the receiver via jp_gnss_rtk_apply_corrections()
 *
 * Below is the skeleton — replace the TODO section with your NTRIP
 * client code or pipe data from an external source.
 */
void* corrections_thread(void* arg)
{
    jp_gnss_hat_t* gnss = (jp_gnss_hat_t*)arg;

    while (atomic_load(&running))
    {
        /* TODO: Receive RTCM3 frames from your NTRIP caster here.
         *
         * Example (pseudocode):
         *
         *   jp_gnss_rtcm3_frame_t frames[16];
         *   uint32_t count = ntrip_client_receive(frames, 16);
         *   if (count > 0)
         *       jp_gnss_rtk_apply_corrections(gnss, frames, count);
         */

        sleep(1);
    }

    return NULL;
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

    printf("GNSS started as RTK Rover\r\n");

    pthread_t corr_thread;
    pthread_create(&corr_thread, NULL, corrections_thread, gnss);

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

    pthread_join(corr_thread, NULL);
    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
