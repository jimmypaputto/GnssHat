/*
 * Jimmy Paputto 2023
 */

#include "ublox/SpiDriver.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <linux/spi/spidev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>


namespace JimmyPaputto
{

SpiDriver::SpiDriver()
:	spiFd_(-1),
    spiDevice_(UBX_SPI_DEV)
{
    currentSpiMode_ = expectedSpiMode;
    init(convertSpiMode(currentSpiMode_));
    std::memset(txBank_, 0xFF, sizeof(txBank_));
}

void SpiDriver::reinit(const ESpiMode spiMode)
{
    deinit();
    currentSpiMode_ = spiMode;
    init(convertSpiMode(currentSpiMode_));
}

void SpiDriver::transmitReceive(std::span<const uint8_t> txBuff,
    std::vector<uint8_t>& rxBuff)
{
    struct spi_ioc_transfer spiTransfer = {};
    spiTransfer.tx_buf = reinterpret_cast<unsigned long>(txBuff.data());
    spiTransfer.rx_buf = reinterpret_cast<unsigned long>(rxBuff.data());
    spiTransfer.len = rxBuff.size();
    spiTransfer.speed_hz = spiSpeed_;
    spiTransfer.bits_per_word = spiBitsPerWord_;

    if (ioctl(spiFd_, SPI_IOC_MESSAGE(1), &spiTransfer) < 0)
    {
        fprintf(stderr, "[SpiDriver] SPI transmit/receive failed\r\n");
        perror("ioctl");
        exit(EXIT_FAILURE);
    }
}

struct spi_ioc_transfer spiTransfer;

void SpiDriver::getRxBuff(uint8_t* rxBuff, const uint32_t size)
{
    spiTransfer.tx_buf = reinterpret_cast<unsigned long>(txBank_);
    spiTransfer.rx_buf = reinterpret_cast<unsigned long>(rxBuff);
    spiTransfer.len = size;
    spiTransfer.speed_hz = spiSpeed_;
    spiTransfer.bits_per_word = spiBitsPerWord_;

    if (ioctl(spiFd_, SPI_IOC_MESSAGE(1), &spiTransfer) < 0)
    {
        fprintf(stderr, "[SpiDriver] getRxBuff SPI transfer failed\r\n");
        perror("ioctl");
        exit(EXIT_FAILURE);
    }
}

void SpiDriver::init(const uint8_t spiMode)
{
    spiFd_ = open(spiDevice_, O_RDWR);
    if (spiFd_ < 0)
    {
        fprintf(stderr, "[SpiDriver] Failed to open SPI device %s\r\n",
            spiDevice_);
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (ioctl(spiFd_, SPI_IOC_WR_MODE, &spiMode) < 0)
    {
        fprintf(stderr, "[SpiDriver] Failed to set SPI mode\r\n");
        perror("ioctl SPI_IOC_WR_MODE");
        exit(EXIT_FAILURE);
    }

    if (ioctl(spiFd_, SPI_IOC_WR_BITS_PER_WORD, &spiBitsPerWord_) < 0)
    {
        fprintf(stderr, "[SpiDriver] Failed to set SPI bits per word\r\n");
        perror("ioctl SPI_IOC_WR_BITS_PER_WORD");
        exit(EXIT_FAILURE);
    }

    if (ioctl(spiFd_, SPI_IOC_WR_MAX_SPEED_HZ, &spiSpeed_) < 0)
    {
        fprintf(stderr, "[SpiDriver] Failed to set SPI speed\r\n");
        perror("ioctl SPI_IOC_WR_MAX_SPEED_HZ");
        exit(EXIT_FAILURE);
    }
}

void SpiDriver::deinit() const
{
    if (spiFd_ >= 0)
    {
        close(spiFd_);
    }
}

uint8_t SpiDriver::convertSpiMode(const ESpiMode spiMode) const
{
    constexpr std::array<uint8_t, 4> spiModes = {
        SPI_MODE_0, SPI_MODE_1, SPI_MODE_2, SPI_MODE_3
    };

    return spiModes[static_cast<uint8_t>(spiMode)];
}

}  // JimmyPaputto
