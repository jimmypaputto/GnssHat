/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_STARTUP_HPP_
#define JIMMY_PAPUTTO_STARTUP_HPP_

#include <span>
#include <vector>

#include "ublox/ICommDriver.hpp"
#include "ublox/IUbloxConfigRegistry.hpp"
#include "ublox/UbxParser.hpp"


namespace JimmyPaputto
{

class IStartupStrategy
{
public:
    virtual ~IStartupStrategy() = default;

    virtual bool execute() = 0;
};

class StartupBase
{
public:
    explicit StartupBase(ICommDriver& commDriver,
        IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser);
    virtual ~StartupBase() = default;

protected:
    virtual bool reconfigureCommPort() = 0;

    void rate2Registers(const uint16_t measurementRate_Hz);
    void timepulsePinConfig2Registers(const TimepulsePinConfig& tpc);
    bool saveCurrentConfigToFlash();

    bool configure(std::span<const uint32_t> keys);

    bool awaitAck(std::span<const uint8_t> payload, EUbxMsg msgType);
    bool verifyConfig(std::span<const uint32_t> keys);
    std::vector<uint8_t> getExpectedValue(const uint32_t key);

    ICommDriver& commDriver_;
    IUbloxConfigRegistry& configRegistry_;
    UbxParser& ubxParser_;
    std::vector<uint32_t> timepulsePinConfigKeys_;
    static constexpr uint32_t rxBuffSize = 1024;
    std::vector<uint8_t> rxBuff_;
    static std::unordered_map<uint32_t, std::vector<uint8_t>>
        expectedConfigValues_;
};

class M9NStartup: public StartupBase, public IStartupStrategy
{
public:
    M9NStartup(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser);
    virtual ~M9NStartup() = default;

    bool execute() override;

private:
    bool reconfigureCommPort() override;
};

class F10TStartup: public StartupBase, public IStartupStrategy
{
public:
    F10TStartup(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser);
    virtual ~F10TStartup() = default;

    bool execute() override;

private:
    bool reconfigureCommPort() override;
};

class F9PStartup: public M9NStartup
{
public:
    F9PStartup(ICommDriver& commDriver, IUbloxConfigRegistry& configRegistry,
        UbxParser& ubxParser);
    virtual ~F9PStartup() = default;

    bool execute() override;

private:
    bool rtkBaseStartup();
    bool rtkRoverStartup();

    bool base_{false};
    bool rover_{false};
};

}  // JimmyPaputto

#endif // JIMMY_PAPUTTO_STARTUP_HPP_
