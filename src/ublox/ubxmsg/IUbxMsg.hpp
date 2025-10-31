/*
 * Jimmy Paputto 2022
 */

#ifndef I_UBX_MSG_HPP_
#define I_UBX_MSG_HPP_

#include <cstdint>
#include <vector>


namespace JimmyPaputto::ubxmsg
{

class IUbxMsg
{
public:
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual void deserialize(const std::vector<uint8_t>& serialized) = 0;

protected:
    static std::vector<uint8_t> buildFrame(std::vector<uint8_t> frame);
};

}  // JimmyPaputto::ubxmsg

#endif  // I_UBX_MSG_HPP_
