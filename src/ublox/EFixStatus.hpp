/*
 * Jimmy Paputto 2021
 */

#ifndef E_FIX_STATUS_HPP_
#define E_FIX_STATUS_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EFixStatus: uint8_t
{
    Void   = 0x00,
    Active = 0x01
};

}  // JimmyPaputto

#endif  // E_FIX_STATUS_HPP_
