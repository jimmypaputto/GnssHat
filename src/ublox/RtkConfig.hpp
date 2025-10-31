/*
 * Jimmy Paputto 2025
 */

#ifndef E_RTK_CONFIG_HPP_
#define E_RTK_CONFIG_HPP_

#include <cstdint>

#include "ERtkMode.hpp"


namespace JimmyPaputto
{

struct BaseConfig
{
    struct SurveyIn
    {
        uint32_t minimumObservationTime_s;
        double requiredPositionAccuracy_m;
    } surveyIn;
};

struct RtkConfig final
{
    ERtkMode mode;
    std::optional<BaseConfig> base;
};

}  // JimmyPaputto

#endif  // E_RTK_CONFIG_HPP_
