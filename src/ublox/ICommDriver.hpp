/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_I_COMM_DRIVER_HPP_
#define JIMMY_PAPUTTO_I_COMM_DRIVER_HPP_

#include <cstdint>
#include <functional>
#include <span>
#include <vector>


namespace JimmyPaputto
{

class ICommDriver
{
public:
    virtual ~ICommDriver() = default;

    virtual void transmitReceive(std::span<const uint8_t> txBuff,
        std::vector<uint8_t>& rxBuff) = 0;
    virtual void getRxBuff(uint8_t* rxBuff, const uint32_t size) = 0;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_I_COMM_DRIVER_HPP_
