/*
 * Jimmy Paputto 2021
 */

#ifndef E_FIX_TYPE_HPP_
#define E_FIX_TYPE_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EFixType: uint8_t
{
    NoFix                 = 0x00,
    DeadReckoningOnly     = 0x01,
    Fix2D                 = 0x02,
    Fix3D                 = 0x03,
    GnssWithDeadReckoning = 0x04,
    TimeOnlyFix           = 0x05
};

}  // JimmyPaputto

#endif  // E_FIX_TYPE_HPP_
