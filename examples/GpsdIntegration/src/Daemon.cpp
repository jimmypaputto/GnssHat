/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <cstdio>
#include <csignal>

#include <jimmypaputto/GnssHat.hpp>


JimmyPaputto::IGnssHat* ubxHat = nullptr;

void signalHandler(int signal)
{
    if (!ubxHat)
    {
        printf("\nSignal received, but ubxHat is not initialized\r\n");
        return;
    }

    printf("\nReceived signal %d, shutting down daemon...\r\n", signal);
    printf("Stopping NMEA forwarding...\n");
    ubxHat->stopForwardForGpsd();
}

auto main() -> int
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    ubxHat = JimmyPaputto::IGnssHat::create();

    JimmyPaputto::GnssConfig config;
    config.measurementRate_Hz = 1;
    config.dynamicModel = JimmyPaputto::EDynamicModel::Portable;
    config.timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.1f },
        .pulseWhenNoFix = std::nullopt,
        .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    config.geofencing = std::nullopt;

    ubxHat->softResetUbloxSom_HotStart();

    printf("Starting GNSS module...\n");
    const bool isStartupDone = ubxHat->start(config);
    if (!isStartupDone)
    {
        printf("Failed to start GNSS module\n");
        delete ubxHat;
        return 1;
    }

    printf("Startup done succesfully\r\n");

    // Start NMEA forwarding for gpsd
    printf("Creating virtual serial port for gpsd...\r\n");
    if (!ubxHat->startForwardForGpsd())
    {
        printf("Failed to start NMEA forwarding\r\n");
        delete ubxHat;
        return 1;
    }

    printf("NMEA forwarding started!\r\n");
    printf("Virtual serial port: %s\r\n", ubxHat->getGpsdDevicePath().c_str());
    printf("To use with gpsd and PPS, run in another terminal:\r\n");
    printf("\tsudo gpsd -N -D5 %s\r\n", ubxHat->getGpsdDevicePath().c_str());
    printf("\tsudo gpsd -N -D5 -S 2222 %s /dev/pps0  # With PPS support\r\n",
        ubxHat->getGpsdDevicePath().c_str());
    printf("\tcgps  # To view gpsd data\r\n");
    printf("\tgpsmon  # To monitor gpsd and PPS\r\n");
    printf("Daemon is running... Press Ctrl+C to stop\r\n");

    // Enter daemon mode - this will block until signal received
    ubxHat->joinForwardForGpsd();

    printf("Daemon stopped by signal\r\n");
    delete ubxHat;
    printf("Clean up completed. Exiting...\r\n");
    return 0;
}
