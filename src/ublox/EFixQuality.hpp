/*
 * Jimmy Paputto 2021
 */

#ifndef E_FIX_QUALITY_HPP_
#define E_FIX_QUALITY_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum EFixQuality: uint8_t
{
    Invalid = 0,
    GpsFix2D3D = 1,
    DGNSS = 2,
    PpsFix = 3,
    FixedRTK = 4,
    FloatRtk = 5,
    DeadReckoning = 6
};

}  // JimmyPaputto

#endif  // E_FIX_QUALITY_HPP_
