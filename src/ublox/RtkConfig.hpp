/*
 * Jimmy Paputto 2025
 */

#ifndef E_RTK_CONFIG_HPP_
#define E_RTK_CONFIG_HPP_

#include <optional>

#include "BaseConfig.hpp"
#include "ERtkMode.hpp"


namespace JimmyPaputto
{

struct RtkConfig final
{
    ERtkMode mode;
    std::optional<BaseConfig> base;
};

}  // JimmyPaputto

#endif  // E_RTK_CONFIG_HPP_
