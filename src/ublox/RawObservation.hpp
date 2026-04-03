/*
 * Jimmy Paputto 2026
 */

#ifndef RAW_OBSERVATION_HPP_
#define RAW_OBSERVATION_HPP_

#include <cstdint>
#include <vector>

#include "SatelliteInfo.hpp"


namespace JimmyPaputto
{

struct RawObservation
{
    double prMes;
    double cpMes;
    float doMes;
    EGnssId gnssId;
    uint8_t svId;
    uint8_t sigId;
    uint8_t freqId;
    uint16_t locktime;
    uint8_t cno;
    uint8_t prStdev;
    uint8_t cpStdev;
    uint8_t doStdev;
    bool prValid;
    bool cpValid;
    bool halfCyc;
    bool subHalfCyc;
};

struct RawMeasurements
{
    double rcvTow;
    uint16_t week;
    int8_t leapS;
    uint8_t numMeas;
    bool leapSecDetermined;
    bool clkReset;
    uint8_t version;
    std::vector<RawObservation> observations;

    static constexpr uint8_t maxNumberOfMeasurements = 128;
};

}  // JimmyPaputto

#endif  // RAW_OBSERVATION_HPP_
