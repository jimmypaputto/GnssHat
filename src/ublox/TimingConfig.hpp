/*
 * Jimmy Paputto 2026
 */

#ifndef TIMING_CONFIG_HPP_
#define TIMING_CONFIG_HPP_

#include <optional>

#include "BaseConfig.hpp"


namespace JimmyPaputto
{

struct TimingConfig final
{
    bool enableTimeMark{false};
    std::optional<BaseConfig> timeBase{};
};

}  // JimmyPaputto

#endif  // TIMING_CONFIG_HPP_
