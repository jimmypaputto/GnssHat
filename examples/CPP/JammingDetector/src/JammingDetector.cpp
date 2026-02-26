/*
 * Jimmy Paputto 2025
 */

#include <cstdio>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

GnssConfig createDefaultConfig()
{
    return GnssConfig {
        .measurementRate_Hz = 5,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt
    };
}

void printRfBlock(const RfBlock& rfBlock)
{
    printf("Band %s\r\n", Utils::eBand2string(rfBlock.id).c_str());
    printf("    Noise per ms: %d\r\n", rfBlock.noisePerMS);
    printf("    AGC monitor, percentage of max gain: %.2f%%\r\n",
        rfBlock.agcMonitor);
    printf("    Antenna status: %s\r\n",
        Utils::antennaStatus2string(rfBlock.antennaStatus).c_str());
    printf("    JammingState: %s\r\n",
        Utils::jammingState2string(rfBlock.jammingState).c_str());
    printf("    CW interference suppression level: %.2f%%\r\n",
        rfBlock.cwInterferenceSuppressionLevel);
}

void analyzeRfBlock(const RfBlock& rfBlock)
{
    constexpr float jammingTreshold = 40;
    if (rfBlock.cwInterferenceSuppressionLevel < jammingTreshold)
    {
        printf(
            "CW interference suppression level is below %.f%%, "
            "no jamming\r\n",
            jammingTreshold
        );
    }
    else
    {
        printf(
            "CW interference suppression level is above %.f%%, "
            "jamming detected\r\n",
            jammingTreshold
        );
    }
}

auto main() -> int
{
    const auto gnssConfig = createDefaultConfig();
    auto* ubxHat = IGnssHat::create();
    ubxHat->softResetUbloxSom_HotStart();
    const auto isStartupDone = ubxHat->start(gnssConfig);
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }

    while (true)
    {
        const auto rfBlocks = ubxHat->waitAndGetFreshNavigation().rfBlocks;
        for (const auto& rfBlock : rfBlocks)
        {
            printRfBlock(rfBlock);
            analyzeRfBlock(rfBlock);
        }
    }

    return 0;
}
