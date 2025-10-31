/*
 * Jimmy Paputto 2022
 */

#include "ublox/Ublox.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "ublox/SpiDriver.hpp"
#include "ublox/UartDriver.hpp"
#include "ublox/ubxmsg/UBX_CFG_CFG.hpp"
#include "ublox/ubxmsg/UBX_CFG_NAV5.hpp"
#include "ublox/ubxmsg/UBX_MON_RF.hpp"
#include "ublox/ubxmsg/UBX_NAV_DOP.hpp"
#include "ublox/ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ublox/ubxmsg/UBX_NAV_PVT.hpp"

#define GNSS_RESET_PIN 4


namespace JimmyPaputto
{

Ublox::Ublox(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
    UbxParser& ubxParser, IStartupStrategy& startup, IRunStrategy& runStrategy,
    Notifier& navigationNotifier)
:   commDriver_(commDriver),
    configRegistry_(configRegistry),
    ubxParser_(ubxParser),
    startup_(startup),
    run_(runStrategy),
    navigationNotifier_(navigationNotifier)
{
}

Ublox::~Ublox()
{
}

void Ublox::powerOnUbloxSom()
{
    setGpio(CHIP_NAME, GNSS_RESET_PIN, 1);
}

void Ublox::powerOffUbloxSom()
{
    setGpio(CHIP_NAME, GNSS_RESET_PIN, 0);
}

bool Ublox::startup()
{
    powerOnUbloxSom();
    return startup_.execute();
}

void Ublox::run()
{
    run_.execute();
}

}  // JimmyPaputto
