/*
 * Jimmy Paputto 2025
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>

using namespace JimmyPaputto;


uint16_t getFrameId(const std::vector<uint8_t>& frame)
{
    if (frame.size() < 6 || frame[0] != 0xD3)
        return 0;

    const uint8_t b0 = frame[3];
    const uint8_t b1 = frame[4];
    const uint16_t msgId = (uint16_t(b0) << 4) | (b1 >> 4);
    return msgId;
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
        .rtk = RtkConfig {
            .mode = ERtkMode::Base,
            .base = BaseConfig {
                .surveyIn = BaseConfig::SurveyIn {
                    .minimumObservationTime_s = 120,
                    .requiredPositionAccuracy_m = 50.0
                }
            }
        }
    };
}

void printFrame(const std::vector<uint8_t>& frame, const std::string& time)
{
    printf("[%s] Frame %zu: %zu bytes: ", 
        time.c_str(), getFrameId(frame), frame.size());

    for (size_t j = 0; j < frame.size(); ++j)
    {
        printf("%02X ", frame[j]);
        if ((j + 1) % 16 == 0 && j + 1 < frame.size())
            printf("\n                                             ");
    }
    printf("\r\n");
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
    const bool isStartupDone = ubxHat->start(createConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    while (true)
    {
        const auto pvt = ubxHat->waitAndGetFreshNavigation().pvt;
        const auto fixStatus = pvt.fixType;
        const auto time = Utils::utcTimeFromGnss_ISO8601(pvt);
        if (fixStatus != EFixType::TimeOnlyFix)
        {
            printf(
                "[%s] Fix status: %s, waiting for TimeOnlyFix for RTK Base\r\n",
                time.c_str(),
                Utils::eFixType2string(fixStatus).c_str()
            );
        }
        else
        {
            printf(
                "[%s] RTK Base ready with TimeOnlyFix\r\n",
                time.c_str()
            );
            const auto rtcm3Frames =
                ubxHat->rtk()->base()->getTinyCorrections();

            std::for_each(
                rtcm3Frames.cbegin(),
                rtcm3Frames.cend(),
                [&time](const auto& f) { printFrame(f, time); }
            );
        }
    }

    return 0;
}
