/*
 * Jimmy Paputto 2026
 */

#ifndef SATELLITE_INFO_HPP_
#define SATELLITE_INFO_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EGnssId : uint8_t
{
    GPS     = 0,
    SBAS    = 1,
    Galileo = 2,
    BeiDou  = 3,
    IMES    = 4,
    QZSS    = 5,
    GLONASS = 6
};

enum class ESvQuality : uint8_t
{
    NoSignal                      = 0,
    Searching                     = 1,
    SignalAcquired                = 2,
    SignalDetectedButUnusable     = 3,
    CodeLockedAndTimeSynchronized = 4,
    CodeAndCarrierLocked1         = 5,
    CodeAndCarrierLocked2         = 6,
    CodeAndCarrierLocked3         = 7
};

struct SatelliteInfo
{
    EGnssId gnssId = {};
    uint8_t svId = {};
    uint8_t cno = {};
    int8_t elevation = {};
    int16_t azimuth = {};
    ESvQuality quality = {};
    bool usedInFix = {};
    bool healthy = {};
    bool diffCorr = {};
    bool ephAvail = {};
    bool almAvail = {};

    static constexpr uint8_t maxNumberOfSatellites = 64;
};

}  // JimmyPaputto

#endif  // SATELLITE_INFO_HPP_
