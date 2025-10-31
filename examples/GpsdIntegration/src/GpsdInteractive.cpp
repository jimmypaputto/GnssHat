/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <cstdio>
#include <csignal>
#include <thread>
#include <atomic>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

std::atomic<bool> running{true};

void signalHandler(int signal)
{
    printf("\nReceived signal %d, shutting down...\r\n", signal);
    running = false;
}

void print(const Navigation& navigation)
{
    printf("Time from GNSS: %s\r\n",
           Utils::utcTimeFromGnss_ISO8601(navigation.pvt).c_str());
    printf("Lat: %f, Lon: %f, Alt: %f\r\n",
           navigation.pvt.latitude,
           navigation.pvt.longitude,
           navigation.pvt.altitude);
}

auto main() -> int
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    auto* ubxHat = IGnssHat::create();

    GnssConfig config;
    config.measurementRate_Hz = 1;
    config.dynamicModel = EDynamicModel::Portable;
    config.timepulsePinConfig = TimepulsePinConfig {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.1f },
        .pulseWhenNoFix = std::nullopt,
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    config.geofencing = std::nullopt;

    const bool isStartupDone = ubxHat->start(config);
    if (!isStartupDone)
    {
        printf("Failed to start GNSS module\r\n");
        return 1;
    }

    printf("Startup done succesfully\r\n");

    if (ubxHat->startForwardForGpsd())
    {
        printf("NMEA forwarding started!\n");
        printf("Virtual serial port: %s\n", ubxHat->getGpsdDevicePath().c_str());
        printf("\nTo use with gpsd, run:\n");
        printf("  sudo gpsd %s\n", ubxHat->getGpsdDevicePath().c_str());
        printf("  cgps  # To view gpsd data\n");
    }
    else
    {
        printf("Failed to start NMEA forwarding\n");
        running = false;
    }

    while (running)
    {
        // You can do whatever you want here, nmea forwarder is running
        // in background without collisions with your application,
        // you can use gpsd simultaneously.
        const auto navigation = ubxHat->waitAndGetFreshNavigation();
        print(navigation);
    }

    ubxHat->stopForwardForGpsd();
    delete ubxHat;

    return 0;
}
