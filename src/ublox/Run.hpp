/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_RUN_HPP_
#define JIMMY_PAPUTTO_RUN_HPP_

#include <thread>

#include "ublox/ICommDriver.hpp"
#include "ublox/Rtcm3Parser.hpp"
#include "ublox/UbxParser.hpp"


namespace JimmyPaputto
{

class IRunStrategy
{
public:
    virtual ~IRunStrategy() = default;

    virtual void execute() = 0;
};

class RunBase
{
public:
    RunBase(ICommDriver& commDriver, UbxParser& ubxParser);
    virtual ~RunBase() = default;

protected:
    ICommDriver& commDriver_;
    UbxParser& ubxParser_;

    static constexpr uint32_t runRxBuffSize = 4096;
    std::vector<uint8_t> runRxBuff_;
    uint32_t runRxBuffOffset_;
};

class M9NRun : public IRunStrategy, public RunBase
{
public:
    M9NRun(ICommDriver& commDriver, UbxParser& ubxParser,
        Notifier& txReadyNotifier, Notifier& navigationNotifier);
    ~M9NRun() override = default;

    void execute() override;

private:
    Notifier& txReadyNotifier_;
    Notifier& navigationNotifier_;
};

class F10TRun : public IRunStrategy, public RunBase
{
public:
    F10TRun(ICommDriver& commDriver, UbxParser& ubxParser);
    ~F10TRun() override = default;

    void execute() override;
};

class F9PRun : public M9NRun
{
public:
    F9PRun(ICommDriver& commDriver, UbxParser& ubxParser,
        Notifier& txReadyNotifier, Notifier& navigationNotifier,
        Rtcm3Store& rtcm3Store, const GnssConfig& config);
    ~F9PRun() override;

private:
    void executeUartBase();
    void executeUartRover();

    std::unique_ptr<ICommDriver> uartDriver_;
    Rtcm3Parser rtcm3Parser_;
    Rtcm3Store& rtcm3Store_;
    std::jthread uart_;

    static constexpr uint32_t uartBuffSize = 2048;
    std::vector<uint8_t> uartBuff_;
    uint32_t uartBuffOffset_;

    static constexpr uint32_t unfinishedFrameBuffMaxSize = 1024;
    std::vector<uint8_t> unfinishedFrameBuff_;
};

}  // JimmyPaputto

#endif // JIMMY_PAPUTTO_RUN_HPP_
