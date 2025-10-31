/*
 * Jimmy Paputto 2022
 */

#ifndef POSITION_VELOCITY_TIME_HPP_
#define POSITION_VELOCITY_TIME_HPP_

#include "EFixQuality.hpp"
#include "EFixStatus.hpp"
#include "EFixType.hpp"


namespace JimmyPaputto
{

struct UTC
{
    uint8_t hh;
    uint8_t mm;
    uint8_t ss;
    bool valid;
    int32_t accuracy;
};

struct Date
{
    uint8_t day;
    uint8_t month;
    uint16_t year;
    bool valid;
};

struct PositionVelocityTime
{
    EFixQuality fixQuality;
    EFixStatus fixStatus;
    EFixType fixType;

    UTC utc;
    Date date;

    float altitude;
    float altitudeMSL;

    double latitude;
    double longitude;

    float speedOverGround;
    float speedAccuracy;
    float heading;
    float headingAccuracy;

    uint8_t visibleSatellites;

    float horizontalAccuracy;
    float verticalAccuracy;
};

}  // JimmyPaputto

#endif  // POSITION_VELOCITY_TIME_HPP_
