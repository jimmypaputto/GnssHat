/*
 * Jimmy Paputto 2022
 */

#ifndef GEOFENCE_HPP_
#define GEOFENCE_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EPioPinPolarity: uint8_t
{
    LowMeansInside  = 0x00,
    LowMeansOutside = 0x01
};

enum class EGeofenceStatus: uint8_t
{
    Unknown = 0x00,
    Inside  = 0x01,
    Outside = 0x02
};

enum class EGeofencingStatus: uint8_t
{
    NotAvalaible = 0x00,
    Active       = 0x01
};

struct Geofence
{
    float lat;
    float lon;
    float radius;

    static constexpr uint8_t size = 12;
};

}  // JimmyPaputto

#endif  // GEOFENCE_HPP_
