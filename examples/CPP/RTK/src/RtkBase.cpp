/*
 * Jimmy Paputto 2026
 */

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>
#include <jimmypaputto/ntrip/NtripCaster.hpp>

using namespace JimmyPaputto;

static std::atomic<bool> g_running{true};

static void signalHandler(int)
{
    g_running = false;
}


uint16_t getFrameId(const std::vector<uint8_t>& frame)
{
    if (frame.size() < 6 || frame[0] != 0xD3)
        return 0;

    const uint8_t b0 = frame[3];
    const uint8_t b1 = frame[4];
    const uint16_t msgId = (uint16_t(b0) << 4) | (b1 >> 4);
    return msgId;
}

GnssConfig createSurveyInConfig()
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
                .mode = BaseConfig::SurveyIn {
                    .minimumObservationTime_s = 120,
                    .requiredPositionAccuracy_m = 50.0
                }
            }
        }
    };
}

GnssConfig createFixedPositionConfig_Lla()
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
                .mode = BaseConfig::FixedPosition {
                    .position = BaseConfig::FixedPosition::Lla {
                        .latitude_deg  = 52.232222222,
                        .longitude_deg = 21.008055556,
                        .height_m      = 110.0
                    },
                    .positionAccuracy_m = 0.5
                }
            }
        }
    };
}

GnssConfig createFixedPositionConfig_Ecef()
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
                .mode = BaseConfig::FixedPosition {
                    .position = BaseConfig::FixedPosition::Ecef {
                        .x_m = 3656215.987,
                        .y_m = 1409547.654,
                        .z_m = 5049982.321
                    },
                    .positionAccuracy_m = 0.5
                }
            }
        }
    };
}

void printFrame(const std::vector<uint8_t>& frame, const std::string& time)
{
    printf("[%s] Frame %u: %zu bytes: ", 
        time.c_str(), getFrameId(frame), frame.size());

    for (size_t j = 0; j < frame.size(); ++j)
    {
        printf("%02X ", frame[j]);
        if ((j + 1) % 16 == 0 && j + 1 < frame.size())
            printf("\n                                             ");
    }
    printf("\r\n");
}

auto main(int argc, char* argv[]) -> int
{
    uint16_t port = 2101;
    std::string mountpoint = "GNSS_HAT";

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (strcmp(argv[i], "--mountpoint") == 0 && i + 1 < argc)
            mountpoint = argv[++i];
    }

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto* ubxHat = IGnssHat::create();
    if (!ubxHat)
    {
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    // ubxHat->softResetUbloxSom_HotStart();
    ubxHat->hardResetUbloxSom_ColdStart();
    const bool isStartupDone = ubxHat->start(createSurveyInConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    NtripCaster caster("0.0.0.0", port, mountpoint);
    if (!caster.start())
    {
        printf("Failed to start NTRIP caster\r\n");
        return -1;
    }

    while (g_running)
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
                ubxHat->rtk()->base()->getFullCorrections();

            std::for_each(
                rtcm3Frames.cbegin(),
                rtcm3Frames.cend(),
                [&time](const auto& f) { printFrame(f, time); }
            );

            caster.feed(rtcm3Frames);
            caster.updatePosition(pvt.latitude, pvt.longitude);

            if (caster.clientCount() > 0)
                printf("[%s] Broadcasting to %zu client(s)\r\n",
                       time.c_str(), caster.clientCount());
        }
    }

    caster.stop();
    return 0;
}
