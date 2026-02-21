/*
 * Jimmy Paputto 2025
 */

#ifndef E_RTK_CONFIG_HPP_
#define E_RTK_CONFIG_HPP_

#include <cstdint>
#include <variant>

#include "ERtkMode.hpp"


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

struct RtkConfig final
{
    ERtkMode mode;
    std::optional<BaseConfig> base;
};

}  // JimmyPaputto

#endif  // E_RTK_CONFIG_HPP_
