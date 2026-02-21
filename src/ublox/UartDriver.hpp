/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_UART_DRIVER_HPP_
#define JIMMY_PAPUTTO_UART_DRIVER_HPP_

#include <cstdint>
#include <vector>

#include "ublox/ICommDriver.hpp"
#include "ublox/ubxmsg/UBX_CFG_PRT.hpp"

#define UBX_UART_DEV "/dev/ttyAMA0"


namespace JimmyPaputto
{

class UartDriver: public ICommDriver
{
public:
    explicit UartDriver();
    ~UartDriver();

    void transmitReceive(std::span<const uint8_t> txBuff,
        std::vector<uint8_t>& rxBuff) override;
    void getRxBuff(uint8_t* rxBuff, const uint32_t size) override;

    void reinit(const uint32_t baudrate);
    uint32_t currentBaudrate() const { return baudrate_; }

    void transmit(std::span<const uint8_t> txBuff);
    int epoll(uint8_t* rxBuff, const uint32_t size, int timeoutMs = -1);

    static constexpr uint32_t expectedBaudrate = 115200;

private:
    void init(const uint32_t baudrate);
    void deinit() const;
    void initEpoll();
    void deinitEpoll();

    uint32_t baudrate_;

    int uartFd_;
    const char* uartDevice_;

    int epollFd_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_UART_DRIVER_HPP_
