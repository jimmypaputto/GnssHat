/*
 * Jimmy Paputto 2026
 */

#include "ublox/BaseConfig.hpp"

#include <cstdio>
#include <variant>


namespace JimmyPaputto
{

bool checkBaseConfig(const BaseConfig& baseConfig)
{
    const auto& mode = baseConfig.mode;

    if (std::holds_alternative<BaseConfig::SurveyIn>(mode))
    {
        const auto& surveyIn = std::get<BaseConfig::SurveyIn>(mode);

        if (surveyIn.minimumObservationTime_s == 0)
        {
            fprintf(
                stderr,
                "[BaseConfig] Invalid survey-in minimum "
                "observation time: 0, should be > 0\r\n"
            );
            return false;
        }

        if (surveyIn.requiredPositionAccuracy_m <= 0.0)
        {
            fprintf(
                stderr,
                "[BaseConfig] Invalid survey-in required "
                "position accuracy: %.2f, should be > 0.0\r\n",
                surveyIn.requiredPositionAccuracy_m
            );
            return false;
        }
    }
    else if (std::holds_alternative<BaseConfig::FixedPosition>(mode))
    {
        const auto& fixed = std::get<BaseConfig::FixedPosition>(mode);

        if (fixed.positionAccuracy_m <= 0.0)
        {
            fprintf(
                stderr,
                "[BaseConfig] Invalid fixed position "
                "accuracy: %.2f, should be > 0.0\r\n",
                fixed.positionAccuracy_m
            );
            return false;
        }

        if (std::holds_alternative<BaseConfig::FixedPosition::Lla>(
                fixed.position))
        {
            const auto& lla =
                std::get<BaseConfig::FixedPosition::Lla>(fixed.position);

            if (lla.latitude_deg < -90.0 || lla.latitude_deg > 90.0)
            {
                fprintf(
                    stderr,
                    "[BaseConfig] Invalid latitude: %f, "
                    "should be -90.0 - 90.0\r\n",
                    lla.latitude_deg
                );
                return false;
            }

            if (lla.longitude_deg < -180.0 || lla.longitude_deg > 180.0)
            {
                fprintf(
                    stderr,
                    "[BaseConfig] Invalid longitude: %f, "
                    "should be -180.0 - 180.0\r\n",
                    lla.longitude_deg
                );
                return false;
            }
        }
    }

    return true;
}

}  // JimmyPaputto
