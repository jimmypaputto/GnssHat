/*
 * Jimmy Paputto 2025
 */

#ifndef E_UBX_MEMORY_LAYER_HPP_
#define E_UBX_MEMORY_LAYER_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EUbxMemoryLayer: std::uint8_t
{
    None    = 0,
    RAM     = 0b001,
    BBR     = 0b010,
    Flash   = 0b100,
    Default = 7
};

}  // JimmyPaputto

#endif  // E_UBX_MEMORY_LAYER_HPP_