/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_BASE_CONFIG_HPP_
#define JIMMY_PAPUTTO_BASE_CONFIG_HPP_

#include <cstdint>
#include <variant>


namespace JimmyPaputto
{

struct BaseConfig
{
    struct SurveyIn
    {
        uint32_t minimumObservationTime_s;
        double requiredPositionAccuracy_m;
    };

    struct FixedPosition
    {
        struct Ecef
        {
            double x_m;
            double y_m;
            double z_m;
        };

        struct Lla
        {
            double latitude_deg;
            double longitude_deg;
            double height_m;
        };

        std::variant<Ecef, Lla> position;
        double positionAccuracy_m;
    };

    std::variant<SurveyIn, FixedPosition> mode;
};

bool checkBaseConfig(const BaseConfig& baseConfig);

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_BASE_CONFIG_HPP_
