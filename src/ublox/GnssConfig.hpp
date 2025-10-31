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
    std::optional<Geofencing> geofencing;

    std::optional<RtkConfig> rtk;
};

bool checkMeasurmentRate(const uint16_t measurmentRate);
bool checkTimepulsePinConfig(const TimepulsePinConfig& timepulsePinConfig);
bool checkGeofencing(const std::optional<GnssConfig::Geofencing>& geofencing);

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_GNSS_CONFIG_HPP_
