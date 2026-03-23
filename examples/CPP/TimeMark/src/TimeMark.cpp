/*
 * Jimmy Paputto 2026
 */

#include <atomic>
#include <cstdio>
#include <thread>

#include <signal.h>
#include <gpiod.h>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

#define EXTINT_PIN 17

std::atomic<bool> running{true};

void signalHandler(int)
{
    running = false;
}

const char* timeBase2string(ETimeMarkTimeBase tb)
{
    switch (tb)
    {
        case ETimeMarkTimeBase::ReceiverTime: return "Receiver";
        case ETimeMarkTimeBase::GnssTime:     return "GNSS";
        case ETimeMarkTimeBase::UTC:          return "UTC";
    }
    return "Unknown";
}

const char* mode2string(ETimeMarkMode m)
{
    switch (m)
    {
        case ETimeMarkMode::Single:  return "Single";
        case ETimeMarkMode::Running: return "Running";
    }
    return "Unknown";
}

const char* run2string(ETimeMarkRun r)
{
    switch (r)
    {
        case ETimeMarkRun::Armed:   return "Armed";
        case ETimeMarkRun::Stopped: return "Stopped";
    }
    return "Unknown";
}

void printTimeMark(const TimeMark& tm)
{
    printf("--- New TimeMark event ---\r\n");
    printf("  channel:          %u\r\n", tm.channel);
    printf("  mode:             %s\r\n", mode2string(tm.mode));
    printf("  run:              %s\r\n", run2string(tm.run));
    printf("  timeBase:         %s\r\n", timeBase2string(tm.timeBase));
    printf("  timeValid:        %s\r\n", tm.timeValid ? "yes" : "no");
    printf("  utcAvailable:     %s\r\n", tm.utcAvailable ? "yes" : "no");
    printf("  newRisingEdge:    %s\r\n", tm.newRisingEdge ? "yes" : "no");
    printf("  newFallingEdge:   %s\r\n", tm.newFallingEdge ? "yes" : "no");
    printf("  count:            %u\r\n", tm.count);
    printf("  rising  WN: %u  TOW: %u ms + %u ns\r\n",
        tm.weekNumberRising, tm.towRising_ms, tm.towSubRising_ns);
    printf("  falling WN: %u  TOW: %u ms + %u ns\r\n",
        tm.weekNumberFalling, tm.towFalling_ms, tm.towSubFalling_ns);
    printf("  accuracy:         %u ns\r\n", tm.accuracyEstimate_ns);
    printf("\r\n");
}

void toggleExtintThread(const char* chipName)
{
    struct gpiod_chip* chip = gpiod_chip_open(chipName);
    if (!chip)
    {
        fprintf(stderr, "[EXTINT] Failed to open %s\r\n", chipName);
        return;
    }

    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    struct gpiod_line_config* line_cfg = gpiod_line_config_new();
    const unsigned int offset = EXTINT_PIN;
    gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

    struct gpiod_request_config* req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "TimeMark_extint");

    struct gpiod_line_request* request =
        gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    if (!request)
    {
        fprintf(stderr, "[EXTINT] Failed to request GPIO %d\r\n", EXTINT_PIN);
        return;
    }

    bool state = false;
    while (running.load())
    {
        state = !state;
        gpiod_line_request_set_value(request, offset,
            state ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
        printf("[EXTINT] GPIO %d -> %s\r\n", EXTINT_PIN,
            state ? "HIGH" : "LOW");

        for (int i = 0; i < 50 && running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_release(request);
}

GnssConfig createConfig()
{
    return GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = std::nullopt,
        .enableTimeMark = true
    };
}

auto main() -> int
{
    signal(SIGINT, signalHandler);

    auto* ubxHat = IGnssHat::create();
    ubxHat->softResetUbloxSom_HotStart();

    const bool isStartupDone = ubxHat->start(createConfig());
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        delete ubxHat;
        return -1;
    }
    printf("Startup done, ublox configured\r\n");
    printf("TimeMark enabled, toggling EXTINT (GPIO %d) every 5s\r\n\r\n",
        EXTINT_PIN);

    const char* chipName = "/dev/gpiochip4";
    std::jthread toggleThread([chipName]([[maybe_unused]] std::stop_token stoken) {
        toggleExtintThread(chipName);
    });

    while (running.load())
    {
        const auto tm = ubxHat->waitAndGetFreshTimeMark();
        if (running.load())
            printTimeMark(tm);
    }

    running = false;
    toggleThread.request_stop();
    printf("Exiting...\r\n");

    delete ubxHat;
    return 0;
}
