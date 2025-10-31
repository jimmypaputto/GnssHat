/*
 * Jimmy Paputto 2022
 */

#include "IUbxMsg.hpp"

#include "ublox/UbxParser.hpp"


namespace JimmyPaputto::ubxmsg
{

std::vector<uint8_t> IUbxMsg::buildFrame(std::vector<uint8_t> frame)
{
    JimmyPaputto::UbxParser::addChecksum(frame);
    return frame;
}

}  // JimmyPaputto::ubxmsg
