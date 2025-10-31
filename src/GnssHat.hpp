/*
 * Jimmy Paputto 2024
 */

#ifndef GNSS_HAT_HPP_
#define GNSS_HAT_HPP_

#include <string>

#include "ublox/GnssConfig.hpp"
#include "ublox/Navigation.hpp"
#include "ublox/RTK.hpp"


namespace JimmyPaputto
{

class IGnssHat
{
public:
    virtual bool start(const GnssConfig& config) = 0;

    virtual Navigation navigation() const = 0;
    virtual Navigation waitAndGetFreshNavigation() = 0;

    virtual void hardResetUbloxSom_ColdStart() const = 0;
    virtual void softResetUbloxSom_HotStart() = 0;

    virtual IRtk* rtk() = 0;

    virtual bool startForwardForGpsd() = 0;
    virtual void stopForwardForGpsd() = 0;
    virtual void joinForwardForGpsd() = 0;
    virtual std::string getGpsdDevicePath() const = 0;

    virtual bool enableTimepulse() = 0;
    virtual void disableTimepulse() = 0;
    virtual void timepulse() = 0;

    static IGnssHat* create();

    virtual ~IGnssHat() = default;
};

namespace Utils
{

std::string eBand2string(const EBand e);
std::string eFixQuality2string(const EFixQuality e);
std::string eFixStatus2string(const EFixStatus e);
std::string eFixType2string(const EFixType e);
std::string jammingState2string(const EJammingState e);
std::string antennaStatus2string(const EAntennaStatus e);
std::string antennaPower2string(const EAntennaPower e);
std::string geofencingStatus2string(const EGeofencingStatus e);
std::string geofenceStatus2string(const EGeofenceStatus e);

std::string utcTimeFromGnss_ISO8601(const PositionVelocityTime& pvt);

}  // Utils

}  // JimmyPaputto

#endif  // GNSS_HAT_HPP_
