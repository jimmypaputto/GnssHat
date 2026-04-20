/*
 * Jimmy Paputto 2026
 *
 * RTK Rover example (C)
 *
 * Demonstrates configuring the GNSS module as an RTK Rover
 * with an integrated NTRIP client that receives RTCM3
 * corrections from a caster and applies them to the receiver.
 *
 * Usage: ./rtk_rover [--host HOST] [--port PORT]
 *                    [--mountpoint MP] [--user U] [--password P]
 */

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


typedef struct
{
    jp_gnss_hat_t* gnss;
    jp_gnss_ntrip_client_t* client;
} corrections_ctx_t;

/*
 * Correction application thread.
 *
 * Receives RTCM3 frames from the NTRIP client and forwards
 * them to the receiver via jp_gnss_rtk_apply_corrections().
 */
void* corrections_thread(void* arg)
{
    corrections_ctx_t* ctx = (corrections_ctx_t*)arg;

    while (atomic_load(&running))
    {
        if (!jp_gnss_ntrip_client_is_connected(ctx->client))
        {
            sleep(1);
            continue;
        }

        jp_gnss_rtcm3_frame_t* frames = NULL;
        uint32_t count = jp_gnss_ntrip_client_receive(
            ctx->client, &frames);

        if (count > 0 && frames)
        {
            jp_gnss_rtk_apply_corrections(ctx->gnss, frames, count);
            jp_gnss_ntrip_client_free_frames(frames, count);
        }
        else
        {
            usleep(100000); /* 100ms */
        }
    }

    return NULL;
}


int main(int argc, char* argv[])
{
    signal(SIGINT, signal_handler);

    /* Defaults */
    const char* host = "localhost";
    uint16_t port = 2101;
    const char* mountpoint = "GNSS_HAT";
    const char* user = "";
    const char* password = "";

    for (int i = 1; i < argc - 1; i += 2)
    {
        if (strcmp(argv[i], "--host") == 0)       host = argv[i + 1];
        else if (strcmp(argv[i], "--port") == 0)  port = (uint16_t)atoi(argv[i + 1]);
        else if (strcmp(argv[i], "--mountpoint") == 0) mountpoint = argv[i + 1];
        else if (strcmp(argv[i], "--user") == 0)  user = argv[i + 1];
        else if (strcmp(argv[i], "--password") == 0) password = argv[i + 1];
    }

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

    jp_gnss_ntrip_client_t* client = jp_gnss_ntrip_client_create(
        host, port, mountpoint, user, password);
    if (!client)
    {
        printf("Failed to create NTRIP client\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    if (!jp_gnss_ntrip_client_connect(client))
    {
        printf("Failed to connect to NTRIP caster %s:%u/%s\r\n",
               host, port, mountpoint);
        jp_gnss_ntrip_client_destroy(client);
        jp_gnss_hat_destroy(gnss);
        return -1;
    }

    corrections_ctx_t ctx = { .gnss = gnss, .client = client };

    pthread_t corr_thread;
    pthread_create(&corr_thread, NULL, corrections_thread, &ctx);

    jp_gnss_navigation_t navigation;

    while (atomic_load(&running))
    {
        if (!jp_gnss_hat_wait_and_get_fresh_navigation(gnss, &navigation))
            continue;

        printf("[%s] %s (%s)  %.6f, %.6f  alt=%.1fm  sats=%d\r\n",
            jp_gnss_utc_time_iso8601(&navigation.pvt),
            jp_gnss_fix_quality_to_string(navigation.pvt.fix_quality),
            jp_gnss_fix_type_to_string(navigation.pvt.fix_type),
            navigation.pvt.latitude, navigation.pvt.longitude,
            navigation.pvt.altitude, navigation.pvt.visible_satellites
        );
    }

    pthread_join(corr_thread, NULL);
    jp_gnss_ntrip_client_disconnect(client);
    jp_gnss_ntrip_client_destroy(client);
    jp_gnss_hat_destroy(gnss);
    printf("Application terminated gracefully\n");

    return 0;
}
