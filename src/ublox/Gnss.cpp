/*
 * Jimmy Paputto 2021
 */

#include "ublox/Gnss.hpp"

#include "common/Utils.hpp"


#define SEMAPHORE_TIMEOUT 100

namespace JimmyPaputto
{

Gnss::Gnss()
{
}

void Gnss::dop(const DilutionOverPrecision& dop)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        navigation_.dop = dop;
        xSemaphore_.releaseResource();
    }
}

void Gnss::pvt(const PositionVelocityTime& pvt)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        navigation_.pvt = pvt;
        xSemaphore_.releaseResource();
    }
}

void Gnss::geofencingCfg(const Geofencing::Cfg& cfg)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        navigation_.geofencing.cfg = cfg;
        xSemaphore_.releaseResource();
    }
}

void Gnss::geofencingNav(const Geofencing::Nav& nav)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        navigation_.geofencing.nav = nav;
        xSemaphore_.releaseResource();
    }
}

void Gnss::rfBlocks(const std::vector<RfBlock>& rfBlocks)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        navigation_.rfBlocks = rfBlocks;
        xSemaphore_.releaseResource();
    }
}

bool Gnss::lock() const
{
    return xSemaphore_.takeResource(SEMAPHORE_TIMEOUT);
}

void Gnss::unlock() const
{
    xSemaphore_.releaseResource();
}

Navigation Gnss::navigation() const
{
    return navigation_;
}

}  // JimmyPaputto
