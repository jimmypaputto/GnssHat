/*
 * Jimmy Paputto 2021
 */

#ifndef E_RTK_MODE_HPP_
#define E_RTK_MODE_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class ERtkMode: uint8_t
{
    Base   = 0x00,
    Rover  = 0x01
};

}  // JimmyPaputto

#endif  // E_RTK_MODE_HPP_
