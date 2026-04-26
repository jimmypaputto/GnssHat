/*
 * Jimmy Paputto 2023
 */

#include "ublox/GnssConfig.hpp"

#include <cstdio>

#include "common/Utils.hpp"


namespace JimmyPaputto
{

bool checkMeasurementRate(const uint16_t measurementRate)
{
    if (measurementRate > 25 || measurementRate < 1)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid measurement rate: %d, "
            "should be 1 - 25 Hz\r\n",
            measurementRate
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

    if (geofencing->geofences.empty())
    {
        fprintf(
            stderr,
            "[GnssConfig] Geofencing enabled but no geofences defined, "
            "use std::nullopt to disable geofencing\r\n"
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

bool checkTiming(const std::optional<TimingConfig>& timing)
{
    if (!timing.has_value())
    {
        return true;
    }

    if (!timing->enableTimeMark && !timing->timeBase.has_value())
    {
        fprintf(
            stderr,
            "[GnssConfig] TimingConfig provided but neither enableTimeMark "
            "nor timeBase is set - use nullopt instead\r\n"
        );
        return false;
    }

    if (timing->timeBase.has_value())
    {
        return checkBaseConfig(timing->timeBase.value());
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

bool checkNavigationFilters(
    const std::optional<GnssConfig::NavigationFilters>& filters)
{
    if (!filters.has_value())
    {
        return true;
    }

    if (filters->minSvs.has_value())
    {
        const auto v = *filters->minSvs;
        if (v < 3 || v > 32)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid navigation filter minSvs: %u, "
                "should be 3 - 32\r\n",
                static_cast<unsigned>(v)
            );
            return false;
        }
    }

    if (filters->maxSvs.has_value())
    {
        const auto v = *filters->maxSvs;
        if (v < 3 || v > 32)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid navigation filter maxSvs: %u, "
                "should be 3 - 32\r\n",
                static_cast<unsigned>(v)
            );
            return false;
        }
    }

    if (filters->minSvs.has_value() && filters->maxSvs.has_value())
    {
        if (*filters->maxSvs < *filters->minSvs)
        {
            fprintf(
                stderr,
                "[GnssConfig] Navigation filter maxSvs (%u) must be >= "
                "minSvs (%u)\r\n",
                static_cast<unsigned>(*filters->maxSvs),
                static_cast<unsigned>(*filters->minSvs)
            );
            return false;
        }
    }

    if (filters->minCno_dBHz.has_value() && *filters->minCno_dBHz > 63)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid navigation filter minCno: %u dBHz, "
            "should be 0 - 63\r\n",
            static_cast<unsigned>(*filters->minCno_dBHz)
        );
        return false;
    }

    if (filters->cnoThrs_dBHz.has_value() && *filters->cnoThrs_dBHz > 63)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid navigation filter cnoThrs: %u dBHz, "
            "should be 0 - 63\r\n",
            static_cast<unsigned>(*filters->cnoThrs_dBHz)
        );
        return false;
    }

    if (filters->minElev_deg.has_value())
    {
        const auto v = *filters->minElev_deg;
        if (v < -90 || v > 90)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid navigation filter minElev: %d deg, "
                "should be -90 - 90\r\n",
                static_cast<int>(v)
            );
            return false;
        }
    }

    if (filters->fixMode.has_value())
    {
        using FixMode = GnssConfig::NavigationFilters::FixMode;
        const auto v = *filters->fixMode;
        if (v != FixMode::Only2D && v != FixMode::Only3D
            && v != FixMode::Auto)
        {
            fprintf(
                stderr,
                "[GnssConfig] Invalid navigation filter fixMode: %u, "
                "should be 1 (2D), 2 (3D) or 3 (Auto)\r\n",
                static_cast<unsigned>(v)
            );
            return false;
        }
    }

    // OUTFIL_PDOP / _TDOP carry a 0.1 DOP unit. u-blox accepts 0 to mean
    // "disabled" so we allow the full U2 range; we still reject obviously
    // nonsensical values above 9999 (999.9 DOP) to catch unit-mix-ups.
    if (filters->pdopMask_x10.has_value() && *filters->pdopMask_x10 > 9999)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid navigation filter pdopMask_x10: %u, "
            "should be 0 - 9999 (0.1 DOP units)\r\n",
            static_cast<unsigned>(*filters->pdopMask_x10)
        );
        return false;
    }
    if (filters->tdopMask_x10.has_value() && *filters->tdopMask_x10 > 9999)
    {
        fprintf(
            stderr,
            "[GnssConfig] Invalid navigation filter tdopMask_x10: %u, "
            "should be 0 - 9999 (0.1 DOP units)\r\n",
            static_cast<unsigned>(*filters->tdopMask_x10)
        );
        return false;
    }

    // OUTFIL_PACC / _TACC are U2 metres; full range accepted (0 disables).

    return true;
}

}  // JimmyPaputto
