/*
 * Jimmy Paputto 2023
 */

#ifndef E_SPI_MODE_HPP_
#define E_SPI_MODE_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class ESpiMode: uint8_t
{
    SpiMode0 = 0b00, // CPOL = 0, CPHA = 0
    SpiMode1 = 0b01, // CPOL = 0, CPHA = 1
    SpiMode2 = 0b10, // CPOL = 1, CPHA = 0
    SpiMode3 = 0b11  // CPOL = 1, CPHA = 1
};

}  // JimmyPaputto

#endif  // E_SPI_MODE_HPP_
