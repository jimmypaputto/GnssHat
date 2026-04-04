/*
 * Jimmy Paputto 2026
 */

#include <cstdio>

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

void printRawObservations(const Navigation& navigation)
{
    const auto& raw = navigation.rawMeasurements;
    const auto& observations = raw.observations;

    printf("=== Raw Measurements (UBX-RXM-RAWX) ===\r\n");
    printf("Receiver TOW: %.3f s  Week: %u  Leap seconds: %d\r\n",
           raw.rcvTow, raw.week, raw.leapS);
    printf("Num meas: %u  Version: %u  Leap sec determined: %s  "
           "Clk reset: %s\r\n",
           raw.numMeas, raw.version,
           raw.leapSecDetermined ? "Yes" : "No",
           raw.clkReset ? "Yes" : "No");
    printf("Observations: %zu\r\n\r\n", observations.size());

    printf("%-10s %-4s %-6s %-6s %-16s %-16s %-14s %-6s %-4s %-4s\r\n",
           "System", "SV", "Sig", "C/N0", "PR (m)", "CP (cyc)",
           "Doppler (Hz)", "Lock", "PR?", "CP?");
    printf("---------- ---- ------ ------ ---------------- "
           "---------------- -------------- ------ ---- ----\r\n");

    for (const auto& obs : observations)
    {
        printf("%-10s %-4u %-6s %-4u dB %-16.3f %-16.3f %-14.3f %-6u %-4s %-4s\r\n",
               gnssIdToString(obs.gnssId),
               obs.svId,
               Utils::gnssSignalId2string(obs.gnssId, obs.sigId).c_str(),
               obs.cno,
               obs.prMes,
               obs.cpMes,
               static_cast<double>(obs.doMes),
               obs.locktime,
               obs.prValid ? "Y" : "N",
               obs.cpValid ? "Y" : "N");
    }

    printf("---------- ---- ---- ------ ---------------- "
           "---------------- -------------- ------ ---- ----\r\n\r\n");
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
    printf("GNSS started successfully. Monitoring raw observations...\r\n\r\n");

    while (true)
    {
        const auto navigation = ubxHat->waitAndGetFreshNavigation();
        printf("\033[2J\033[H");
        printRawObservations(navigation);
    }

    return 0;
}
