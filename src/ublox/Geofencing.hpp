/*
 * Jimmy Paputto 2022
 */

#ifndef GEOFENCING_HPP_
#define GEOFENCING_HPP_

#include <array>
#include <vector>

#include "Geofence.hpp"


namespace JimmyPaputto
{

struct Geofencing
{
    struct Cfg
    {
        uint8_t pioPinNumber;
        EPioPinPolarity pinPolarity;
        bool pioEnabled;
        uint8_t confidenceLevel;
        std::vector<Geofence> geofences;
    } cfg;

    struct Nav
    {
        explicit Nav()
        :   iTOW(0),
            geofencingStatus(EGeofencingStatus::NotAvalaible),
            numberOfGeofences(0),
            combinedState(EGeofenceStatus::Unknown)
        {
            geofencesStatus.fill(EGeofenceStatus::Unknown);
        }

        uint32_t iTOW;
        EGeofencingStatus geofencingStatus;
        uint8_t numberOfGeofences;
        EGeofenceStatus combinedState;
        std::array<EGeofenceStatus, 4> geofencesStatus;
    } nav;
};

}  // JimmyPaputto

#endif  // GEOFENCING_HPP_
