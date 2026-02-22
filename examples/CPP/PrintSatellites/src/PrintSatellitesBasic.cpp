/*
 * Jimmy Paputto 2026
 */

#include <chrono>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

const char* gnssIdToString(EGnssId id)
{
    switch (id)
    {
        case EGnssId::GPS:     return "GPS";
        case EGnssId::SBAS:    return "SBAS";
        case EGnssId::Galileo: return "Galileo";
        case EGnssId::BeiDou:  return "BeiDou";
        case EGnssId::IMES:    return "IMES";
        case EGnssId::QZSS:    return "QZSS";
        case EGnssId::GLONASS: return "GLONASS";
        default:               return "Unknown";
    }
}

const char* svQualityToString(ESvQuality quality)
{
    switch (quality)
    {
        case ESvQuality::NoSignal:                      return "No Signal";
        case ESvQuality::Searching:                     return "Searching";
        case ESvQuality::SignalAcquired:                return "Acquired";
        case ESvQuality::SignalDetectedButUnusable:     return "Unusable";
        case ESvQuality::CodeLockedAndTimeSynchronized: return "Code Locked";
        case ESvQuality::CodeAndCarrierLocked1:         return "Carrier Lock 1";
        case ESvQuality::CodeAndCarrierLocked2:         return "Carrier Lock 2";
        case ESvQuality::CodeAndCarrierLocked3:         return "Carrier Lock 3";
        default:                                        return "Unknown";
    }
}

void printSatellites(const Navigation& navigation)
{
    const auto& satellites = navigation.satellites;

    printf("=== Satellite Information ===\r\n");
    printf("Total satellites: %zu\r\n", satellites.size());
    printf("%-10s %-6s %-8s %-12s %-10s %-14s %-10s %-8s\r\n",
           "System", "SV ID", "C/N0", "Elevation", "Azimuth", "Quality", "Used",
           "Health");
    printf("---------- ------ -------- ------------ ---------- -------------- "
           "---------- --------\r\n");

    uint8_t usedCount = 0;
    for (const auto& sat : satellites)
    {
        if (sat.usedInFix)
            usedCount++;

        printf(
            "%-10s %-6d %-5d dB  %5d°       %5d°     %-14s %-10s %-8s\r\n",
            gnssIdToString(sat.gnssId),
            sat.svId,
            sat.cno,
            sat.elevation,
            sat.azimuth,
            svQualityToString(sat.quality),
            sat.usedInFix ? "Yes" : "No",
            sat.healthy ? "OK" : "Bad"
        );
    }

    printf("---------- ------ -------- ------------ ---------- --------------"
           "---------- --------\r\n");
    printf(
        "Satellites used in fix: %d / %zu\r\n\n",
        usedCount,
        satellites.size()
    );
}

GnssConfig createDefaultConfig()
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
        .rtk = std::nullopt
    };
}

auto main() -> int
{
    auto* ubxHat = IGnssHat::create();
    if (!ubxHat)
    {
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    ubxHat->softResetUbloxSom_HotStart();
    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring satellite data...\r\n\n");

    while (true)
    {
        const auto navigation = ubxHat->waitAndGetFreshNavigation();
        printSatellites(navigation);
    }

    return 0;
}
