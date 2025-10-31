/*
 * Jimmy Paputto 2022
 */

#ifndef E_DYNAMIC_MODEL_HPP_
#define E_DYNAMIC_MODEL_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EDynamicModel: uint8_t
{
    Portable   = 0,
    Stationary = 2,
    Pedestrain = 3,
    Automotive = 4,
    Sea        = 5,
    Airborne1G = 6,
    Airborne2G = 7,
    Airborne4G = 8,
    Wrist      = 9,
    Bike       = 10,
    Mower      = 11,
    Escooter   = 12
};

}  // JimmyPaputto

#endif  // E_DYNAMIC_MODEL_HPP_
