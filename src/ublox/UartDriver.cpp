/*
 * Jimmy Paputto 2025
 */

#include "ublox/UartDriver.hpp"

#include <cstring>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>


namespace JimmyPaputto
{

UartDriver::UartDriver()
:	uartFd_(-1),
    uartDevice_(UBX_UART_DEV),
    baudrate_(expectedBaudrate),
    epollFd_(-1)
{
    init(baudrate_);
    initEpoll();
}

UartDriver::~UartDriver()
{
    deinitEpoll();
    deinit();
}

void UartDriver::reinit(const uint32_t baudrate)
{
    deinit();
    baudrate_ = baudrate;
    init(baudrate_);
}

void UartDriver::transmitReceive(std::span<const uint8_t> txBuff,
    std::vector<uint8_t>& rxBuff)
{
    if (uartFd_ < 0)
    {
        printf("[UART] Error: UART not initialized\r\n");
        return;
    }

    if (txBuff.empty())
    {
        return;
    }

    const ssize_t written = write(uartFd_, txBuff.data(), txBuff.size());
    if (written != txBuff.size())
    {
        printf(
            "[UART] Error: Failed to write all data, expected: %zu, "
            "written: %d\n",
            txBuff.size(),
            written
        );
    }

    getRxBuff(rxBuff.data(), rxBuff.size());
}

void UartDriver::getRxBuff(uint8_t* rxBuff, const uint32_t size)
{
    if (uartFd_ < 0 || !rxBuff)
        return;

    const ssize_t readed = read(uartFd_, rxBuff, size);

    if (readed < 0)
    {
        printf("[UART] getRxBuff Error: Failed to read data from UART\r\n");
        perror("[UART] getRxBuff perror");
        std::memset(rxBuff, 0xFF, size);
    }
    else if (readed < size)
    {
        std::memset(rxBuff + readed, 0xFF, size - readed);
    }
    else if (readed == 0)
    {
        printf("[UART] getRxBuff Warning: No data available to read\r\n");
        std::memset(rxBuff, 0xFF, size);
    }
}

void UartDriver::init(const uint32_t baudrate)
{
    uartFd_ = open(uartDevice_, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uartFd_ < 0)
    {
        perror("[UART] Failed to open UART device");
        return;
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (tcgetattr(uartFd_, &tty) != 0)
    {
        perror("[UART] Failed to get terminal attributes");
        close(uartFd_);
        uartFd_ = -1;
        return;
    }

    speed_t speed;
    switch (baudrate)
    {
        case 9600U:   speed = B9600;   break;
        case 19200U:  speed = B19200;  break;
        case 38400U:  speed = B38400;  break;
        case 57600U:  speed = B57600;  break;
        case 115200U: speed = B115200; break;
        default:
            printf("[UART] Error: Unsupported baud rate: %u\n", baudrate);
            close(uartFd_);
            uartFd_ = -1;
            return;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Configure 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;        // No parity
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;         // Clear size mask
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable read, ignore control lines

    // Configure input flags
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);           // No software flow control
    tty.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);   // Raw input

    // Configure output flags
    tty.c_oflag &= ~OPOST;         // Raw output

    // Configure line discipline
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);   // Raw mode

    // Set timeouts
    tty.c_cc[VMIN] = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 1;  // 0.1 second timeout

    // Apply terminal attributes
    if (tcsetattr(uartFd_, TCSANOW, &tty) != 0)
    {
        perror("[UART] Error: Failed to set terminal attributes");
        close(uartFd_);
        uartFd_ = -1;
        return;
    }

    // Flush any existing data
    tcflush(uartFd_, TCIOFLUSH);

    baudrate_ = baudrate;
}

void UartDriver::deinit() const
{
    if (uartFd_ >= 0)
    {
        close(uartFd_);
    }
}

void UartDriver::transmit(std::span<const uint8_t> txBuff)
{
    if (uartFd_ < 0)
    {
        printf("[UART] Error: UART not initialized\r\n");
        return;
    }

    if (txBuff.empty())
    {
        return;
    }

    const ssize_t written = write(uartFd_, txBuff.data(), txBuff.size());
    if (written != txBuff.size())
    {
        printf(
            "[UART] Error: Failed to write all data, expected: %zu, "
            "written: %d\n",
            txBuff.size(),
            written
        );
    }
}

void UartDriver::initEpoll()
{
    if (uartFd_ < 0)
    {
        printf("[UART] Cannot init epoll - UART not initialized\n");
        return;
    }

    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0)
    {
        perror("[UART] Failed to create epoll instance");
        return;
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = uartFd_;
    
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, uartFd_, &event) < 0)
    {
        perror("[UART] Failed to add UART fd to epoll");
        close(epollFd_);
        epollFd_ = -1;
        return;
    }
    
}

void UartDriver::deinitEpoll()
{
    if (epollFd_ >= 0)
    {
        close(epollFd_);
        epollFd_ = -1;
    }
}

int UartDriver::epoll(uint8_t* rxBuff, const uint32_t size, int timeoutMs)
{
    if (epollFd_ < 0 || uartFd_ < 0)
    {
        printf("[UART] epoll or UART not initialized\n");
        return -1;
    }

    constexpr int maxEvents = 12;
    struct epoll_event events[maxEvents];
    const int numEvents = epoll_wait(epollFd_, events, maxEvents, timeoutMs);

    if (numEvents < 0)
    {
        if (errno != EINTR)
            perror("[UART] epoll_wait failed");
        return -1;
    }

    if (numEvents == 0)
        return 0;

    ssize_t totalBytesRead = 0;
    if (events[0].data.fd == uartFd_ && (events[0].events & EPOLLIN))
    {
        const auto bytesRead = read(
            uartFd_,
            rxBuff + totalBytesRead,
            size - totalBytesRead
        );

        if (bytesRead > 0)
        {
            totalBytesRead += bytesRead;
        }
        else if (bytesRead < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("[UART] Read error in epoll");
            }
        }
    }

    return totalBytesRead;
}

}  // JimmyPaputto
