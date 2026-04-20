/*
 * Jimmy Paputto 2026
 */

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
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

void print_time_mark(const jp_gnss_time_mark_t* tm)
{
    printf("--- New TimeMark event ---\r\n");
    printf("  channel:          %u\r\n", tm->channel);
    printf("  mode:             %s\r\n",
        jp_gnss_time_mark_mode_to_string(tm->mode));
    printf("  run:              %s\r\n",
        jp_gnss_time_mark_run_to_string(tm->run));
    printf("  timeBase:         %s\r\n",
        jp_gnss_time_mark_time_base_to_string(tm->time_base));
    printf("  timeValid:        %s\r\n", tm->time_valid ? "yes" : "no");
    printf("  utcAvailable:     %s\r\n", tm->utc_available ? "yes" : "no");
    printf("  newRisingEdge:    %s\r\n", tm->new_rising_edge ? "yes" : "no");
    printf("  newFallingEdge:   %s\r\n",
        tm->new_falling_edge ? "yes" : "no");
    printf("  count:            %u\r\n", tm->count);
    printf("  rising  WN: %u  TOW: %u ms + %u ns\r\n",
        tm->week_number_rising, tm->tow_rising_ms, tm->tow_sub_rising_ns);
    printf("  falling WN: %u  TOW: %u ms + %u ns\r\n",
        tm->week_number_falling, tm->tow_falling_ms,
        tm->tow_sub_falling_ns);
    printf("  accuracy:         %u ns\r\n", tm->accuracy_estimate_ns);
    printf("\r\n");
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
    config.timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;

    config.has_timing = true;
    config.timing.enable_time_mark = true;

    return config;
}

void* toggle_thread_func(void* arg)
{
    jp_gnss_hat_t* gnss = (jp_gnss_hat_t*)arg;

    while (atomic_load(&running))
    {
        jp_gnss_hat_trigger_time_mark(gnss,
            JP_GNSS_TIME_MARK_TRIGGER_EDGE_TOGGLE);
        printf("[EXTINT] toggled\r\n");

        for (int i = 0; i < 50 && atomic_load(&running); ++i)
            usleep(100000);
    }

    return NULL;
}

void* time_mark_thread_func(void* arg)
{
    jp_gnss_hat_t* gnss = (jp_gnss_hat_t*)arg;

    while (atomic_load(&running))
    {
        jp_gnss_time_mark_t tm;
        if (!jp_gnss_hat_wait_and_get_fresh_time_mark(gnss, &tm))
            continue;

        if (!atomic_load(&running))
            return NULL;

        print_time_mark(&tm);
    }

    return NULL;
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
    printf("Startup done, ublox configured\r\n");

    if (!jp_gnss_hat_enable_time_mark_trigger(gnss))
    {
        printf("Failed to enable TimeMark trigger\r\n");
        jp_gnss_hat_destroy(gnss);
        return -1;
    }
    printf("TimeMark trigger enabled, toggling EXTINT every 5s\r\n\r\n");

    pthread_t toggle_tid;
    pthread_t time_mark_tid;
    pthread_create(&toggle_tid, NULL, toggle_thread_func, gnss);
    pthread_create(&time_mark_tid, NULL, time_mark_thread_func, gnss);

    while (atomic_load(&running))
        usleep(100000);

    jp_gnss_hat_disable_time_mark_trigger(gnss);
    jp_gnss_hat_destroy(gnss);

    printf("Exiting...\r\n");
    pthread_join(toggle_tid, NULL);
    pthread_join(time_mark_tid, NULL);

    return 0;
}
