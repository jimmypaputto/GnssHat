/*
 * Jimmy Paputto 2023
 */

#include "ublox/GnssConfig.hpp"

#include <cstdio>

#include "common/Utils.hpp"


namespace JimmyPaputto
{

bool checkMeasurmentRate(const uint16_t measurmentRate)
{
    if (measurmentRate > 25 || measurmentRate < 1)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid measurement rate: %d, "
            "should be 1 - 25 Hz\r\n",
            measurmentRate
        );
        return false;
    }
    return true;
}

bool checkGeofencing(const std::optional<GnssConfig::Geofencing>& geofencing)
{
    if (!geofencing.has_value())
    {
        return true;
    }

    if (geofencing->confidenceLevel > 5)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid confidence level: %d, "
            "should be 0 (lowest) - 5 (highest)\r\n",
            geofencing->confidenceLevel
        );
        return false;
    }

    if (geofencing->geofences.size() > 4)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid number of geofences: %d, should be 0 - 4\r\n",
            static_cast<int>(geofencing->geofences.size())
        );
        return false;
    }

    for (const auto& geofence : geofencing->geofences)
    {
        if (geofence.lat > 90 || geofence.lat < -90)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid geofence latitude: %f, "
                "should be -90.0 - 90.0\r\n",
                geofence.lat
            );
            return false;
        }

        if (geofence.lon > 180 || geofence.lon < -180)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid geofence longitude: %f, "
                "should be -180.0 - 180.0\r\n",
                geofence.lon
            );
            return false;
        }

        if (geofence.radius <= 0.0)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid radius: %.2f, should be >0.00\r\n",
                geofence.radius
            );
            return false;
        }
    }

    return true;
}

bool checkTimepulsePinConfig(const TimepulsePinConfig& timepulsePinConfig)
{
    if (!timepulsePinConfig.active)
    {
        return true;
    }

    const auto& fixedPulse = timepulsePinConfig.fixedPulse;
    if (fixedPulse.pulseWidth < 0.0 || fixedPulse.pulseWidth >= 1)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid timepulse pulse width: %.2f, "
            "should be 0.0 - 0.99\r\n",
            fixedPulse.pulseWidth
        );
        return false;
    }

    if (!timepulsePinConfig.pulseWhenNoFix.has_value())
    {
        return true;
    }
    const auto& pulseWhenNoFix = *timepulsePinConfig.pulseWhenNoFix;
    if (pulseWhenNoFix.pulseWidth < 0.0 || pulseWhenNoFix.pulseWidth >= 1)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid timepulse pulse width when no fix: %.2f, "
            "should be 0.0 - 0.99\r\n",
            pulseWhenNoFix.pulseWidth
        );
        return false;
    }

    return true;
}

}  // JimmyPaputto
