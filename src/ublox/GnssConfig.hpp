/*
 * Jimmy Paputto 2022
 */

#ifndef JIMMY_PAPUTTO_GNSS_CONFIG_HPP_
#define JIMMY_PAPUTTO_GNSS_CONFIG_HPP_

#include <optional>
#include <vector>

#include "EDynamicModel.hpp"
#include "Geofence.hpp"
#include "RtkConfig.hpp"
#include "TimepulsePinConfig.hpp"
#include "TimingConfig.hpp"


namespace JimmyPaputto
{

struct GnssConfig
{
    uint16_t measurementRate_Hz;

    EDynamicModel dynamicModel;

    TimepulsePinConfig timepulsePinConfig;

    struct Geofencing
    {
        std::vector<Geofence> geofences;
        uint8_t confidenceLevel;
        std::optional<EPioPinPolarity> pioPinPolarity;
    };
    std::optional<Geofencing> geofencing{};

    std::optional<RtkConfig> rtk{};

    std::optional<TimingConfig> timing{};

    struct NavigationFilters
    {
        std::optional<uint8_t> minSvs{};
        std::optional<uint8_t> maxSvs{};
        std::optional<uint8_t> minCno_dBHz{};
        std::optional<int8_t>  minElev_deg{};
        std::optional<uint8_t> nCnoThrs{};
        std::optional<uint8_t> cnoThrs_dBHz{};

        enum class FixMode : uint8_t
        {
            Only2D = 1,
            Only3D = 2,
            Auto   = 3,
        };
        std::optional<FixMode> fixMode{};

        std::optional<uint16_t> pdopMask_x10{};
        std::optional<uint16_t> tdopMask_x10{};
        std::optional<uint16_t> pAccMask_m{};
        std::optional<uint16_t> tAccMask_m{};
    };
    std::optional<NavigationFilters> navigationFilters{};

    bool saveToFlash{false};
};

bool checkMeasurementRate(const uint16_t measurementRate);
bool checkTimepulsePinConfig(const TimepulsePinConfig& timepulsePinConfig);
bool checkGeofencing(const std::optional<GnssConfig::Geofencing>& geofencing);
bool checkTiming(const std::optional<TimingConfig>& timing);
bool checkNavigationFilters(
    const std::optional<GnssConfig::NavigationFilters>& filters);

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_GNSS_CONFIG_HPP_
