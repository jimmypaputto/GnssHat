/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_FACTORY_HPP_
#define UBX_FACTORY_HPP_

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "EUbxMsg.hpp"
#include "ubxmsg/IUbxMsg.hpp"


namespace JimmyPaputto
{

class UbxFactory
{
public:
    static ubxmsg::IUbxMsg& create(const EUbxMsg& eUbxMsg,
        const std::vector<uint8_t>& frame);

private:
    explicit UbxFactory();

    std::array<ubxmsg::IUbxMsg*, numberOfUbxMsgs> create_;
};

}  // JimmyPaputto

#endif  // UBX_FACTORY_HPP_
