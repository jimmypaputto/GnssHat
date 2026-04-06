/*
 * Jimmy Paputto 2025
 */

#include "Run.hpp"

#include "UartDriver.hpp"


namespace JimmyPaputto
{

RunBase::RunBase(ICommDriver& commDriver, UbxParser& ubxParser)
:   commDriver_(commDriver),
    ubxParser_(ubxParser),
    runRxBuff_(runRxBuffSize),
    runRxBuffOffset_(0)
{
}

M9NRun::M9NRun(ICommDriver& commDriver, UbxParser& ubxParser,
    Notifier& txReadyNotifier, Notifier& navigationNotifier)
:   RunBase(commDriver, ubxParser),
    txReadyNotifier_(txReadyNotifier),
    navigationNotifier_(navigationNotifier)
{
}

void M9NRun::execute(std::stop_token stoken)
{
    constexpr uint32_t rxBatchSize = 1024;
    uint32_t counter = runRxBuffOffset_;
    if (!txReadyNotifier_.wait(stoken))
        return;

    while (!txReadyNotifier_.getFlag())
    {
        if (counter + rxBatchSize > runRxBuffSize)
            counter = 0;
        commDriver_.getRxBuff(runRxBuff_.data() + counter, rxBatchSize);
        counter += rxBatchSize;
    }

    const auto unfinishedFrame = ubxParser_.parse(
        std::vector<uint8_t>(runRxBuff_.data(), runRxBuff_.data() + counter)
    );
    txReadyNotifier_.setFlag(false);
    std::copy(
        unfinishedFrame.begin(), unfinishedFrame.end(), runRxBuff_.begin()
    );
    runRxBuffOffset_ = unfinishedFrame.size();

    navigationNotifier_.notify();
}

F10TRun::F10TRun(ICommDriver& commDriver, UbxParser& ubxParser)
:   RunBase(commDriver, ubxParser)
{
}

void F10TRun::execute(std::stop_token)
{    
    auto& uartDriver = static_cast<UartDriver&>(commDriver_);
    constexpr uint32_t parseThreshold = 100;
    const int epollTimeoutMs = (runRxBuffOffset_ > 0) ? 10 : 500;

    const auto incomingBytes = uartDriver.epoll(
        runRxBuff_.data() + runRxBuffOffset_,
        runRxBuffSize - runRxBuffOffset_,
        epollTimeoutMs
    );

    if (incomingBytes > 0)
        runRxBuffOffset_ += incomingBytes;

    const bool shouldParse =
        (runRxBuffOffset_ >= parseThreshold) ||
        (incomingBytes <= 0 && runRxBuffOffset_ > 0);

    if (!shouldParse)
        return;

    // TODO: ogar tej alokacji, std::span
    const auto unfinishedFrame = ubxParser_.parse(
        std::vector<uint8_t>(
            runRxBuff_.data(),
            runRxBuff_.data() + runRxBuffOffset_
        )
    );

    std::copy(
        unfinishedFrame.begin(), unfinishedFrame.end(), runRxBuff_.begin()
    );
    runRxBuffOffset_ = unfinishedFrame.size();
}

F9PRun::F9PRun(ICommDriver& commDriver, UbxParser& ubxParser,
    Notifier& txReadyNotifier, Notifier& navigationNotifier,
    Rtcm3Store& rtcm3Store, const GnssConfig& config)
:   M9NRun(commDriver, ubxParser, txReadyNotifier, navigationNotifier),
    uartDriver_(std::make_unique<UartDriver>()),
    rtcm3Parser_(rtcm3Store),
    rtcm3Store_(rtcm3Store),
    uartBuff_(uartBuffSize),
    uartBuffOffset_(0)
{
    if (!config.rtk.has_value())
        return;

    const auto& rtkConfig = config.rtk.value();
    if (rtkConfig.mode == ERtkMode::Base && rtkConfig.base.has_value())
    {
        unfinishedFrameBuff_.reserve(unfinishedFrameBuffMaxSize);
        uart_ = std::jthread([this] (std::stop_token stoken) {
            while (!stoken.stop_requested())
            {
                executeUartBase();
            }
        });
    }
    else if (rtkConfig.mode == ERtkMode::Rover)
    {
        uart_ = std::jthread([this](std::stop_token stoken) {
            while (!stoken.stop_requested())
            {
                executeUartRover(stoken);
            }
        });
    }
}

F9PRun::~F9PRun()
{
    if (uart_.joinable())
    {
        uart_.request_stop();
        uart_.join();
    }
}

void F9PRun::executeUartBase()
{
    auto& uartDriver = static_cast<UartDriver&>(*uartDriver_);
    constexpr int epollTimeoutMs = 500;
    const auto incomingBytes = uartDriver.epoll(
        uartBuff_.data() + uartBuffOffset_,
        uartBuffSize - uartBuffOffset_,
        epollTimeoutMs
    );

    if (incomingBytes <= 0)
        return;

    uartBuffOffset_ += incomingBytes;

    rtcm3Parser_.parse(
        std::span<uint8_t>(uartBuff_.data(), uartBuffOffset_),
        unfinishedFrameBuff_
    );

    uartBuffOffset_ = unfinishedFrameBuff_.size();

    if (unfinishedFrameBuff_.empty())
    {
        return;
    }

    std::copy(
        unfinishedFrameBuff_.begin(),
        unfinishedFrameBuff_.end(),
        uartBuff_.begin()
    );
    unfinishedFrameBuff_.clear();
}

void F9PRun::executeUartRover(std::stop_token stoken)
{
    auto& uartDriver = static_cast<UartDriver&>(*uartDriver_);
    const auto incomingFrames = rtcm3Store_.waitForFrames(stoken);
    for (const auto& frame : incomingFrames)
    {
        uartDriver.transmit(frame);
    }
}

}  // JimmyPaputto
