/*
 * Jimmy Paputto 2022
 */

#ifndef UBLOX_HPP_
#define UBLOX_HPP_

#include <memory>

#include "EDynamicModel.hpp"
#include "GnssConfig.hpp"
#include "ICommDriver.hpp"
#include "Run.hpp"
#include "Startup.hpp"
#include "UbloxConfigRegistry.hpp"
#include "UbxParser.hpp"

#include "ubxmsg/UBX_CFG_MSG.hpp"
#include "ubxmsg/UBX_CFG_PRT.hpp"
#include "ubxmsg/UBX_CFG_RATE.hpp"
#include "ubxmsg/UBX_CFG_TP5.hpp"
#include "ubxmsg/UBX_NAV_GEOFENCE.hpp"

#include "common/Notifier.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto
{

class Ublox
{
public:
    explicit Ublox(
        ICommDriver& commDriver,
        IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser,
        IStartupStrategy& startup,
        IRunStrategy& runStrategy,
        Notifier& navigationNotifier
    );
    ~Ublox();

    bool startup();
    void run();

    static void powerOnUbloxSom();
    static void powerOffUbloxSom();

private:
    ICommDriver& commDriver_;
    IUbloxConfigRegistry& configRegistry_;
    UbxParser& ubxParser_;
    IStartupStrategy& startup_;
    IRunStrategy& run_;
    Notifier& navigationNotifier_;
};

}  // JimmyPaputto

#endif  // UBLOX_HPP_
