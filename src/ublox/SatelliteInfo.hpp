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
    GLONASS = 6,
    NAVIC   = 7
};

enum class EGPSSignalId : uint8_t
{
    GPS_L1CA = 0,
    GPS_L2CL = 3,
    GPS_L2CM = 4,
    GPS_L5I  = 6,
    GPS_L5Q  = 7
};

enum class ESBASSignalId : uint8_t
{
    L1CA = 0
};

enum class EGalileoSignalId : uint8_t
{
    E1C  = 0,
    E1B  = 1,
    E5aI = 3,
    E5aQ = 4,
    E5bI = 5,
    E5bQ = 6
};

enum class EBeiDouSignalId : uint8_t
{
    B1ID1 = 0,
    B1ID2 = 1,
    B2ID1 = 2,
    B2ID2 = 3,
    B1Cp  = 5,
    B1Cd  = 6,
    B2ap  = 7,
    B2ad  = 8
};

enum class EQZSSSignalId : uint8_t
{
    L1CA = 0,
    L1S  = 1,
    L2CM = 4,
    L2CL = 5,
    L5I  = 8,
    L5Q  = 9
};

enum class EGLONASSSignalId : uint8_t
{
    L1OF = 0,
    L2OF = 2
};

enum class ENavICSignalId : uint8_t
{
    L5A = 0
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
