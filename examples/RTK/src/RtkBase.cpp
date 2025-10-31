/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


uint16_t getFrameId(const std::vector<uint8_t>& frame)
{
    if (frame.size() < 6 || frame[0] != 0xD3)
        return 0;

    const uint8_t b0 = frame[3];
    const uint8_t b1 = frame[4];
    const uint16_t msgId = (uint16_t(b0) << 4) | (b1 >> 4);
    return msgId;
}

JimmyPaputto::GnssConfig createConfig()
{
    return JimmyPaputto::GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = JimmyPaputto::EDynamicModel::Stationary,
        .timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
            .active = true,
            .fixedPulse = JimmyPaputto::TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = JimmyPaputto::RtkConfig {
            .mode = JimmyPaputto::ERtkMode::Base,
            .base = JimmyPaputto::BaseConfig {
                .surveyIn = JimmyPaputto::BaseConfig::SurveyIn {
                    .minimumObservationTime_s = 120,
                    .requiredPositionAccuracy_m = 50.0
                }
            }
        }
    };
}

auto main() -> int
{
    auto* ubxHat = JimmyPaputto::IGnssHat::create();
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
        const auto time = JimmyPaputto::Utils::utcTimeFromGnss_ISO8601(pvt);
        if (fixStatus != JimmyPaputto::EFixType::TimeOnlyFix)
        {
            printf(
                "[%s] Fix status: %s, waiting for TimeOnlyFix for RTK Base\r\n",
                time.c_str(),
                JimmyPaputto::Utils::eFixType2string(fixStatus).c_str()
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

            for (size_t i = 0; i < rtcm3Frames.size(); ++i)
            {
                const auto& frame = rtcm3Frames[i];
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
        }
    }

    return 0;
}
