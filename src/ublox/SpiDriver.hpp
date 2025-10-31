/*
 * Jimmy Paputto 2021
 */

#ifndef SPI_DRIVER_HPP_
#define SPI_DRIVER_HPP_

#include <cstdint>
#include <vector>

#include "ublox/ESpiMode.hpp"
#include "ublox/ICommDriver.hpp"
#include "ublox/ubxmsg/UBX_CFG_PRT.hpp"

#define UBX_SPI_DEV "/dev/spidev0.0"


namespace JimmyPaputto
{

class SpiDriver: public ICommDriver
{
public:
    explicit SpiDriver();

    void transmitReceive(const std::vector<uint8_t>& txBuff,
        std::vector<uint8_t>& rxBuff) override;
    void getRxBuff(uint8_t* rxBuff, const uint32_t size) override;

    void reinit(const ESpiMode spiMode);
    ESpiMode currentSpiMode() const { return currentSpiMode_; }
    static ubxmsg::UBX_CFG_PRT_SPI portConfig();

    static constexpr ESpiMode expectedSpiMode = ESpiMode::SpiMode0;

private:
    void init(const uint8_t spiMode);
    void deinit() const;
    uint8_t convertSpiMode(const ESpiMode spiMode) const;

    ESpiMode currentSpiMode_;

    int spiFd_;
    const char* spiDevice_;
    uint8_t txBank_[4096];
    static constexpr uint32_t spiSpeed_ = 5'000'000; // 5'000'000 = 5MHz
    static constexpr uint8_t spiBitsPerWord_ = 8;
};

}  // JimmyPaputto

#endif  // SPI_DRIVER_HPP_
