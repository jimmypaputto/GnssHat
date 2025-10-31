/*
 * Jimmy Paputto 2023
 */

#ifndef NAVIGATION_HPP_
#define NAVIGATION_HPP_

#include "DilutionOverPrecision.hpp"
#include "Geofencing.hpp"
#include "PositionVelocityTime.hpp"
#include "RFBlock.hpp"


namespace JimmyPaputto
{

struct Navigation
{
    DilutionOverPrecision dop;
    PositionVelocityTime pvt;
    Geofencing geofencing;
    std::vector<RfBlock> rfBlocks;
};

}  // JimmyPaputto

#endif  // NAVIGATION_HPP_
