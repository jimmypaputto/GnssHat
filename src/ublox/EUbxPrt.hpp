/*
 * Jimmy Paputto 2022
 */

#ifndef E_UBX_PRT_HPP_
#define E_UBX_PRT_HPP_

#include <cstdint>

#include "common/Utils.hpp"


namespace JimmyPaputto
{

enum EUbxPrt: uint8_t
{
    UBX_I2C      = 0x00,
    UBX_UART_1   = 0x01,
    UBX_UART_2   = 0x02,
    UBX_USB      = 0x03,
    UBX_SPI      = 0x04,
    UBX_Reserved = 0x05
};

constexpr const uint8_t numberOfUbxPrts =
    countEnum<EUbxPrt, EUbxPrt::UBX_I2C, EUbxPrt::UBX_Reserved>();

}  // JimmyPaputto

#endif  // E_UBX_PRT_HPP_
