/*
 * Jimmy Paputto 2022
 */

#ifndef JP_UBX_CALLBACKS_HPP_
#define JP_UBX_CALLBACKS_HPP_

#include <functional>
#include <memory>
#include <vector>

#include "EUbxMsg.hpp"
#include "IUbloxConfigRegistry.hpp"
#include "ubxmsg/IUbxMsg.hpp"
#include "common/Notifier.hpp"


namespace JimmyPaputto
{

class UbxCallbacks
{
public:
    explicit UbxCallbacks(IUbloxConfigRegistry& configRegistry,
        Notifier& navigationNotifier, const bool callbackNotificationEnabled);

    void run(ubxmsg::IUbxMsg& ubxMsg, const EUbxMsg& eUbxMsg);

private:
    void ackNakCb(ubxmsg::IUbxMsg& ubxMsg, EUbxMsg eUbxMsg,
        IUbloxConfigRegistry& configRegistry);

    std::vector<std::function<void(ubxmsg::IUbxMsg&)>> callbacks_;
    Notifier& navigationNotifier_;
    const bool callbackNotificationEnabled_;
};

}  // JimmyPaputto

#endif  // JP_UBX_CALLBACKS_HPP_
