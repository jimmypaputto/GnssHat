/*
 * Jimmy Paputto 2021
 */

#ifndef JP_UBX_PARSER_HPP_
#define JP_UBX_PARSER_HPP_

#include <array>
#include <memory>
#include <span>
#include <vector>

#include "IUbloxConfigRegistry.hpp"
#include "UbxCallbacks.hpp"
#include "common/Notifier.hpp"
#include "ubxmsg/IUbxMsg.hpp"


namespace JimmyPaputto
{

class UbxParser final
{
public:
    explicit UbxParser(IUbloxConfigRegistry& configRegistry,
        Notifier& navigationNotifier, bool callbackNotificationEnabled = true);

    std::vector<uint8_t> parse(std::span<const uint8_t> buffer);

    static void addChecksum(std::vector<uint8_t>& frame);
    static std::array<uint8_t, 2> checksum(std::span<const uint8_t> frame,
        uint8_t offset = 2);
    static bool checkFrame(std::span<const uint8_t> frame);

private:
    void extractFrames(std::span<const uint8_t> buffer);

    constexpr static uint16_t maxNumberOfFrames_ = 300;
    std::array<std::vector<uint8_t>, maxNumberOfFrames_> frames_;
    std::array<std::vector<uint8_t>, maxNumberOfFrames_>::iterator endFrameIt_;
    std::vector<uint8_t> unfinishedFrameFromBuffer_;
    IUbloxConfigRegistry& configRegistry_;
    UbxCallbacks ubxCallbacks_;
};

}  // JimmyPaputto

#endif  // JP_UBX_PARSER_HPP_
