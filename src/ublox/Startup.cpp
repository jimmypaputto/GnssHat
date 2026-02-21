/*
 * Jimmy Paputto 2025
 */

#include "ublox/Startup.hpp"

#include <chrono>
#include <cmath>
#include <thread>
#include <variant>

#include "common/Utils.hpp"
#include "ublox/SpiDriver.hpp"
#include "ublox/UartDriver.hpp"
#include "ublox/UbxCfgKeys.hpp"
#include "ublox/ubxmsg/UBX_CFG_CFG.hpp"
#include "ublox/ubxmsg/UBX_CFG_NAV5.hpp"
#include "ublox/ubxmsg/UBX_CFG_VALGET.hpp"
#include "ublox/ubxmsg/UBX_CFG_VALSET.hpp"
#include "ublox/ubxmsg/UBX_MON_RF.hpp"
#include "ublox/ubxmsg/UBX_NAV_DOP.hpp"
#include "ublox/ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ublox/ubxmsg/UBX_NAV_PVT.hpp"


namespace JimmyPaputto
{

StartupBase::StartupBase(ICommDriver& commDriver,
    IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser)
:   commDriver_(commDriver),
    configRegistry_(configRegistry),
    ubxParser_(ubxParser),
    rxBuff_(rxBuffSize)
{
}

bool StartupBase::configurePorts(const std::vector<uint8_t>& serializedPoll,
    const std::vector<uint8_t>& serializedConfig)
{
    const auto configurePortsImpl_ =
        [this, &serializedPoll, &serializedConfig]() -> bool {
            if (checkPortsConfig(serializedPoll))
            {
                return true;
            }

            if (!sendPortsConfig(serializedConfig))
            {
                return false;
            }

            return checkPortsConfig(serializedPoll);
        };

    return try3times(configurePortsImpl_);
}

bool StartupBase::checkPortsConfig(const std::vector<uint8_t>& serializedPoll)
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_PRT)];
    ack = false;
    commDriver_.transmitReceive(serializedPoll, rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack && configRegistry_.isPortConfigured();  // ogar
}

bool StartupBase::sendPortsConfig(const std::vector<uint8_t>& serializedConfig)
{
    configRegistry_.shouldSaveConfigToFlash(true);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_PRT)];
    ack = false;
    commDriver_.transmitReceive(serializedConfig, rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

bool StartupBase::configureRate()
{
    if (checkRateConfig())
    {
        return true;
    }

    if (!sendRateConfig())
    {
        return false;
    }

    return checkRateConfig();
}

bool StartupBase::checkRateConfig()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_RATE)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_RATE::poll(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack && configRegistry_.isRateCorrect();
}

bool StartupBase::sendRateConfig()
{
    configRegistry_.shouldSaveConfigToFlash(true);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_RATE)];
    ack = false;
    const auto& serializedRateConfig =
        configRegistry_.rateConfig().serialize();
    commDriver_.transmitReceive(serializedRateConfig, rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

bool StartupBase::configureTimepulse()
{
    if (checkTimepulseConfig())
    {
        return true;
    }

    if (!sendTimepulseConfig())
    {
        return false;
    }

    return checkTimepulseConfig();
}

bool StartupBase::checkTimepulseConfig()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_TP5)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_TP5::poll(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack && configRegistry_.isTimepulseConfigCorrect();
}

bool StartupBase::sendTimepulseConfig()
{
    configRegistry_.shouldSaveConfigToFlash(true);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_TP5)];
    ack = false;
    const auto& serializedTimepulseConfig =
        configRegistry_.timepulseConfig().serialize();
    commDriver_.transmitReceive(serializedTimepulseConfig, rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

bool StartupBase::configureGeofences()
{
    if (checkGeofencesConfig())
    {
        return true;
    }

    if (!sendGeofencesConfig())
    {
        return false;
    }

    return checkGeofencesConfig();
}

bool StartupBase::checkGeofencesConfig()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_GEOFENCE)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_GEOFENCE::poll(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack && configRegistry_.isGeofencingCorrect();
}

bool StartupBase::sendGeofencesConfig()
{
    configRegistry_.shouldSaveConfigToFlash(true);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_GEOFENCE)];
    ack = false;
    commDriver_.transmitReceive(
        configRegistry_.geofencingConfig().serialize(), rxBuff_
    );
    ubxParser_.parse(rxBuff_);
    return ack;
}

bool StartupBase::configureDynamicModel()
{
    if (checkDynamicModel())
    {
        return true;
    }

    if (!sendDynamicModel())
    {
        return false;
    }

    return checkDynamicModel();
}

bool StartupBase::checkDynamicModel()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_NAV5)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_NAV5::poll(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack && configRegistry_.isDynamicModelCorrect();
}

bool StartupBase::sendDynamicModel()
{
    configRegistry_.shouldSaveConfigToFlash(true);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_NAV5)];
    ack = false;
    const ubxmsg::UBX_CFG_NAV5 ubxCfgNav5(configRegistry_.dynamicModel());
    commDriver_.transmitReceive(ubxCfgNav5.serialize(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

template<typename UbxMsg, EUbxMsg eUbxMsg>
bool StartupBase::configureUbxMsgSendrate()
{
    if (checkUbxMsgSendrate<UbxMsg, eUbxMsg>())
    {
        return true;
    }

    if (!sendUbxMsgSendrate<UbxMsg, eUbxMsg>())
    {
        return false;
    }

    return checkUbxMsgSendrate<UbxMsg, eUbxMsg>();
}

template<typename UbxMsg, EUbxMsg eUbxMsg>
bool StartupBase::checkUbxMsgSendrate()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_MSG)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_MSG::poll(eUbxMsg), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack && configRegistry_.isMsgSendrateCorrect();  // podejrzane, ogar
}

template<typename UbxMsg, EUbxMsg eUbxMsg>
bool StartupBase::sendUbxMsgSendrate()
{
    configRegistry_.shouldSaveConfigToFlash(true);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_MSG)];
    ack = false;
    const auto& serializedMsgCfg = configRegistry_.cfgMsg(eUbxMsg).serialize();
    commDriver_.transmitReceive(serializedMsgCfg, rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

bool StartupBase::saveCurrentConfigToFlash()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_CFG)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_CFG::saveToFlash(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

M9NStartup::M9NStartup(ICommDriver& commDriver,
    IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser)
:	StartupBase(commDriver, configRegistry, ubxParser)
{
}

bool M9NStartup::execute()
{
    bool result = false;

    auto flusher = std::vector<uint8_t>(4096, 0xFF);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = configurePorts(
        ubxmsg::UBX_CFG_PRT::poll<EUbxPrt::UBX_SPI>(),
        SpiDriver::portConfig().serialize()
    );
    if (!result)
    {
        result = reconfigureCommPort();

        if (!result)
        {
            fprintf(stderr, "[Startup] Port configuration failed\r\n");
            return false;
        }
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this](){ return configureDynamicModel(); });
    if (!result)
    {
        fprintf(stderr, "[Startup] Dynamic model configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this](){ return configureGeofences(); });
    if (!result)
    {
        fprintf(stderr, "[Startup] Geofences configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this](){ return configureRate(); });
    if (!result)
    {
        fprintf(stderr, "[Startup] Rate configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this](){ return configureTimepulse(); });
    if (!result)
    {
        fprintf(stderr, "[Startup] Timepulse configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this]() {
        return configureUbxMsgSendrate<ubxmsg::UBX_MON_RF, EUbxMsg::UBX_MON_RF>();
    });
    if (!result)
    {
        fprintf(stderr, "[Startup] UBX_MON_RF configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this](){
        return configureUbxMsgSendrate<ubxmsg::UBX_NAV_DOP, EUbxMsg::UBX_NAV_DOP>();
    });
    if (!result)
    {
        fprintf(stderr, "[Startup] UBX_NAV_DOP configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this]() {
        return configureUbxMsgSendrate<ubxmsg::UBX_NAV_GEOFENCE, EUbxMsg::UBX_NAV_GEOFENCE>();
    });
    if (!result)
    {
        fprintf(stderr, "[Startup] UBX_NAV_GEOFENCE configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = try3times([this]() {
        return configureUbxMsgSendrate<ubxmsg::UBX_NAV_PVT, EUbxMsg::UBX_NAV_PVT>();
    });
    if (!result)
    {
        fprintf(stderr, "[Startup] UBX_NAV_PVT configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    if (configRegistry_.shouldSaveConfigToFlash())
    {
        result = try3times([this](){ return saveCurrentConfigToFlash(); });
        if (!result)
        {
            fprintf(
                stderr,
                "[Startup] Save current configuration to flash failed\r\n"
            );
            return false;
        }
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    return true;
}

bool M9NStartup::reconfigureCommPort()
{
    bool result = false;

    constexpr std::array<ESpiMode, 3> spiModesToCheck = {
        ESpiMode::SpiMode1,
        ESpiMode::SpiMode2,
        ESpiMode::SpiMode3
    };

    auto& spiDriver = static_cast<SpiDriver&>(commDriver_);

    for (const auto& spiMode : spiModesToCheck)
    {
        spiDriver.reinit(spiMode);
        try3times([this](){
            return configurePorts(
                ubxmsg::UBX_CFG_PRT::poll<EUbxPrt::UBX_SPI>(),
                SpiDriver::portConfig().serialize()
            );
        });
        spiDriver.reinit(SpiDriver::expectedSpiMode);
        result = try3times([this](){
            return configurePorts(
                ubxmsg::UBX_CFG_PRT::poll<EUbxPrt::UBX_SPI>(),
                SpiDriver::portConfig().serialize()
            );
        });
        if (result)
        {
            break;
        }
    }

    return result;
}

F10TStartup::F10TStartup(ICommDriver& commDriver,
    IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser)
:	StartupBase(commDriver, configRegistry, ubxParser)
{}

enum class CFG_UART1_STOPBITS : uint8_t
{
    HALF    = 0,
    ONE     = 1,
    ONEHALF = 2,
    TWO     = 3
};

enum class CFG_UART1_DATABITS : uint8_t
{
    EIGHT = 0,
    SEVEN = 1
};

enum class CFG_UART1_PARITY : uint8_t
{
    NONE = 0,
    ODD  = 1,
    EVEN = 2
};

enum class CFG_TMODE_MODE : uint8_t
{
    DISABLED  = 0,
    SURVEY_IN = 1,
    FIXED     = 2
};

std::unordered_map<uint32_t, std::vector<uint8_t>> StartupBase::expectedConfigValues_ = {
    {UbxCfgKeys::CFG_UART1_ENABLED,  {0x01}},
    {UbxCfgKeys::CFG_UART1_BAUDRATE, {0x00, 0xC2, 0x01, 0x00}},  // 115200
    {UbxCfgKeys::CFG_UART1_DATABITS, {static_cast<uint8_t>(CFG_UART1_DATABITS::EIGHT)}},
    {UbxCfgKeys::CFG_UART1_PARITY,   {static_cast<uint8_t>(CFG_UART1_PARITY::NONE)}},
    {UbxCfgKeys::CFG_UART1_STOPBITS, {static_cast<uint8_t>(CFG_UART1_STOPBITS::ONE)}},

    {UbxCfgKeys::CFG_UART1OUTPROT_UBX,  {0x01}},
    {UbxCfgKeys::CFG_UART1OUTPROT_NMEA, {0x00}},

    {UbxCfgKeys::CFG_UART2_ENABLED,  {0x01}},
    {UbxCfgKeys::CFG_UART2_BAUDRATE, {0x00, 0xC2, 0x01, 0x00}},  // 115200
    {UbxCfgKeys::CFG_UART2_DATABITS, {static_cast<uint8_t>(CFG_UART1_DATABITS::EIGHT)}},
    {UbxCfgKeys::CFG_UART2_PARITY,   {static_cast<uint8_t>(CFG_UART1_PARITY::NONE)}},
    {UbxCfgKeys::CFG_UART2_STOPBITS, {static_cast<uint8_t>(CFG_UART1_STOPBITS::ONE)}},

    {UbxCfgKeys::CFG_UART2INPROT_RTCM3X, {0x01}},

    {UbxCfgKeys::CFG_UART2OUTPROT_UBX,    {0x00}},
    {UbxCfgKeys::CFG_UART2OUTPROT_NMEA,   {0x00}},
    {UbxCfgKeys::CFG_UART2OUTPROT_RTCM3X, {0x01}},

    {UbxCfgKeys::CFG_TXREADY_ENABLED,   {0x01}},
    {UbxCfgKeys::CFG_TXREADY_POLARITY,  {0x00}},
    {UbxCfgKeys::CFG_TXREADY_PIN,       {0x07}},
    {UbxCfgKeys::CFG_TXREADY_THRESHOLD, {0x01, 0x00}},

    {UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_UART1,  {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_UART1, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_UART1, {0x01}},

    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1005_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1074_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1077_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1084_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1087_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1094_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1097_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1124_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1127_UART2, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1230_UART2, {0x01}},
};

std::vector<uint8_t> StartupBase::getExpectedValue(const uint32_t key)
{
    const auto it = StartupBase::expectedConfigValues_.find(key);
    if (it != StartupBase::expectedConfigValues_.end())
        return it->second;

    return {};
}

bool StartupBase::configure(const std::vector<uint32_t>& keys)
{
    configRegistry_.clearStoredConfigValues();

    const auto serializedPoll = ubxmsg::UBX_CFG_VALGET::poll(keys);
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_VALGET)];
    ack = false;
    std::fill(rxBuff_.begin(), rxBuff_.end(), 0);
    commDriver_.transmitReceive(serializedPoll, rxBuff_);
    ubxParser_.parse(rxBuff_);

    if (!ack)
    {
        bool result = try3times([this, &ack]() {
            commDriver_.getRxBuff(rxBuff_.data(), rxBuff_.size());
            ubxParser_.parse(rxBuff_);
            return ack;
        });

        if (!result)
        {
            fprintf(stderr, "[Startup] Polling configuration failed\r\n");
            return false;
        }
    }

    std::vector<ubxmsg::ConfigKeyValue> keysValuesToReconfigure;
    for (const auto& key : keys)
    {
        const auto expectedValue = getExpectedValue(key);
        if (expectedValue.empty())
            return false;

        const auto actualValue = configRegistry_.getStoredConfigValue(key);
        if (actualValue != expectedValue)
        {
            fprintf(stderr, "[Startup] Key 0x%08X value mismatch\r\n", key);
            keysValuesToReconfigure.push_back(
                ubxmsg::ConfigKeyValue { .key = key, .value = expectedValue }
            );
        }
    }

    if (keysValuesToReconfigure.empty())
        return true;

    configRegistry_.shouldSaveConfigToFlash(true);

    const auto serializedValset = ubxmsg::UBX_CFG_VALSET(
        0x00,
        EUbxMemoryLayer::RAM,
        keysValuesToReconfigure
    ).serialize();

    bool& set_ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_VALSET)];
    set_ack = false;
    std::fill(rxBuff_.begin(), rxBuff_.end(), 0);
    commDriver_.transmitReceive(serializedValset, rxBuff_);
    ubxParser_.parse(rxBuff_);

    if (!set_ack)
    {
        bool result = try3times([this, &set_ack]() {
            commDriver_.getRxBuff(rxBuff_.data(), rxBuff_.size());
            ubxParser_.parse(rxBuff_);
            return set_ack;
        });

        if (!result)
        {
            fprintf(stderr, "[Startup] Polling configuration failed\r\n");
            return false;
        }
    }

    configRegistry_.clearStoredConfigValues();

    ack = false;
    std::fill(rxBuff_.begin(), rxBuff_.end(), 0);
    commDriver_.transmitReceive(serializedPoll, rxBuff_);
    ubxParser_.parse(rxBuff_);

    if (!ack)
    {
        bool result = try3times([this, &ack]() {
            commDriver_.getRxBuff(rxBuff_.data(), rxBuff_.size());
            ubxParser_.parse(rxBuff_);
            return ack;
        });

        if (!result)
        {
            fprintf(stderr, "[Startup] Polling configuration failed\r\n");
            return false;
        }
    }

    for (const auto& key : keys)
    {
        const auto expectedValue = getExpectedValue(key);
        if (expectedValue.empty())
            return false;

        const auto actualValue = configRegistry_.getStoredConfigValue(key);
        if (actualValue != expectedValue)
        {
            fprintf(stderr, "[Startup] Key 0x%08X value mismatch\r\n", key);
            return false;
        }
    }

    return true;
}

bool F10TStartup::execute()
{
    bool result = false;
    configRegistry_.shouldSaveConfigToFlash(false);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::vector<uint32_t> uart1Keys = {
        UbxCfgKeys::CFG_UART1_ENABLED,
        UbxCfgKeys::CFG_UART1_BAUDRATE,
        UbxCfgKeys::CFG_UART1_DATABITS,
        UbxCfgKeys::CFG_UART1_PARITY,
        UbxCfgKeys::CFG_UART1_STOPBITS
    };
    result = configure(uart1Keys);
    if (!result)
    {
        result = reconfigureCommPort();
        if (!result)
            return false;
    }

    std::vector<uint32_t> txReadyKeys = {
        UbxCfgKeys::CFG_TXREADY_ENABLED,
        UbxCfgKeys::CFG_TXREADY_PIN,
        UbxCfgKeys::CFG_TXREADY_POLARITY,
        UbxCfgKeys::CFG_TXREADY_THRESHOLD,
    };
    result = configure(txReadyKeys);
    if (!result)
        return false;

    std::vector<uint32_t> prtOutProtoKeys = {
        UbxCfgKeys::CFG_UART1OUTPROT_UBX,
        UbxCfgKeys::CFG_UART1OUTPROT_NMEA
    };
    result = configure(prtOutProtoKeys);
    if (!result)
        return false;

    std::vector<uint32_t> msgCfgKeys = {
        UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_UART1,
        UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_UART1,
        UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_UART1
    };
    result = configure(msgCfgKeys);
    if (!result)
        return false;

    if (configRegistry_.shouldSaveConfigToFlash())
    {
        result = try3times([this](){ return saveCurrentConfigToFlash(); });
        if (!result)
        {
            fprintf(
                stderr,
                "[Startup] Save current configuration to flash failed\r\n"
            );
            return false;
        }
    }

    return true;
}

bool F10TStartup::reconfigureCommPort()
{
    bool result = false;

    constexpr std::array<uint32_t, 2> baudratesToCheck = {
        38400,
        115200,
    };

    auto& uartDriver = static_cast<UartDriver&>(commDriver_);

    std::vector<uint32_t> uart1Keys = {
        UbxCfgKeys::CFG_UART1_ENABLED,
        UbxCfgKeys::CFG_UART1_BAUDRATE,
        UbxCfgKeys::CFG_UART1_DATABITS,
        UbxCfgKeys::CFG_UART1_PARITY,
        UbxCfgKeys::CFG_UART1_STOPBITS
    };

    for (const auto& baudrate : baudratesToCheck)
    {
        uartDriver.reinit(baudrate);
        try3times([this, &uart1Keys](){ return configure(uart1Keys); });
        uartDriver.reinit(UartDriver::expectedBaudrate);
        result = try3times([this, &uart1Keys](){
            return configure(uart1Keys);
        });
        if (result)
        {
            break;
        }
    }

    return result;
}

F9PStartup::F9PStartup(ICommDriver& commDriver,
    IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser)
:   M9NStartup(commDriver, configRegistry, ubxParser)
{
    auto config = configRegistry.getGnssConfig();
    if (config.rtk == std::nullopt)
        return;

    base_ = config.rtk->mode == ERtkMode::Base && config.rtk->base.has_value();
    if (!base_)
    {
        rover_ = true;
        return;
    }

    auto& ecv = StartupBase::expectedConfigValues_;
    const auto& baseMode = config.rtk->base->mode;

    if (std::holds_alternative<BaseConfig::SurveyIn>(baseMode))
    {
        const auto& surveyIn = std::get<BaseConfig::SurveyIn>(baseMode);
        ecv[UbxCfgKeys::CFG_TMODE_MODE] =
            {static_cast<uint8_t>(CFG_TMODE_MODE::SURVEY_IN)};
        ecv[UbxCfgKeys::CFG_TMODE_SVIN_MIN_DUR] =
            serializeInt2LittleEndian<uint32_t>(
                surveyIn.minimumObservationTime_s
            );
        ecv[UbxCfgKeys::CFG_TMODE_SVIN_ACC_LIMIT] =
            serializeInt2LittleEndian<uint32_t>(static_cast<uint32_t>(
                surveyIn.requiredPositionAccuracy_m * 10000.0
            ));
    }
    else if (std::holds_alternative<BaseConfig::FixedPosition>(baseMode))
    {
        const auto& fixed = std::get<BaseConfig::FixedPosition>(baseMode);
        ecv[UbxCfgKeys::CFG_TMODE_MODE] =
            {static_cast<uint8_t>(CFG_TMODE_MODE::FIXED)};
        ecv[UbxCfgKeys::CFG_TMODE_FIXED_POS_ACC] =
            serializeInt2LittleEndian<uint32_t>(static_cast<uint32_t>(
                fixed.positionAccuracy_m * 10000.0
            ));

        if (std::holds_alternative<BaseConfig::FixedPosition::Ecef>(
                fixed.position))
        {
            const auto& ecef =
                std::get<BaseConfig::FixedPosition::Ecef>(fixed.position);
            ecv[UbxCfgKeys::CFG_TMODE_POS_TYPE] = {0x00};

            const int64_t x_01mm = std::llround(ecef.x_m * 10000.0);
            const int64_t y_01mm = std::llround(ecef.y_m * 10000.0);
            const int64_t z_01mm = std::llround(ecef.z_m * 10000.0);

            ecv[UbxCfgKeys::CFG_TMODE_ECEF_X] =
                serializeInt2LittleEndian<int32_t>(
                    static_cast<int32_t>(x_01mm / 100));
            ecv[UbxCfgKeys::CFG_TMODE_ECEF_X_HP] =
                serializeInt2LittleEndian<int8_t>(
                    static_cast<int8_t>(x_01mm % 100));
            ecv[UbxCfgKeys::CFG_TMODE_ECEF_Y] =
                serializeInt2LittleEndian<int32_t>(
                    static_cast<int32_t>(y_01mm / 100));
            ecv[UbxCfgKeys::CFG_TMODE_ECEF_Y_HP] =
                serializeInt2LittleEndian<int8_t>(
                    static_cast<int8_t>(y_01mm % 100));
            ecv[UbxCfgKeys::CFG_TMODE_ECEF_Z] =
                serializeInt2LittleEndian<int32_t>(
                    static_cast<int32_t>(z_01mm / 100));
            ecv[UbxCfgKeys::CFG_TMODE_ECEF_Z_HP] =
                serializeInt2LittleEndian<int8_t>(
                    static_cast<int8_t>(z_01mm % 100));
        }
        else if (std::holds_alternative<BaseConfig::FixedPosition::Lla>(
                     fixed.position))
        {
            const auto& lla =
                std::get<BaseConfig::FixedPosition::Lla>(fixed.position);
            ecv[UbxCfgKeys::CFG_TMODE_POS_TYPE] = {0x01};

            const int64_t lat_1e9 = std::llround(lla.latitude_deg * 1e9);
            const int64_t lon_1e9 = std::llround(lla.longitude_deg * 1e9);
            const int64_t h_01mm  = std::llround(lla.height_m * 10000.0);

            ecv[UbxCfgKeys::CFG_TMODE_LAT] =
                serializeInt2LittleEndian<int32_t>(
                    static_cast<int32_t>(lat_1e9 / 100));
            ecv[UbxCfgKeys::CFG_TMODE_LAT_HP] =
                serializeInt2LittleEndian<int8_t>(
                    static_cast<int8_t>(lat_1e9 % 100));
            ecv[UbxCfgKeys::CFG_TMODE_LON] =
                serializeInt2LittleEndian<int32_t>(
                    static_cast<int32_t>(lon_1e9 / 100));
            ecv[UbxCfgKeys::CFG_TMODE_LON_HP] =
                serializeInt2LittleEndian<int8_t>(
                    static_cast<int8_t>(lon_1e9 % 100));
            ecv[UbxCfgKeys::CFG_TMODE_HEIGHT] =
                serializeInt2LittleEndian<int32_t>(
                    static_cast<int32_t>(h_01mm / 100));
            ecv[UbxCfgKeys::CFG_TMODE_HEIGHT_HP] =
                serializeInt2LittleEndian<int8_t>(
                    static_cast<int8_t>(h_01mm % 100));
        }
    }
}

bool F9PStartup::execute()
{
    bool result = M9NStartup::execute();
    if (!result)
        return false;

    configRegistry_.shouldSaveConfigToFlash(false);

    const std::vector<uint32_t> uart2Keys = {
        UbxCfgKeys::CFG_UART2_ENABLED,
        UbxCfgKeys::CFG_UART2_BAUDRATE,
        UbxCfgKeys::CFG_UART2_DATABITS,
        UbxCfgKeys::CFG_UART2_PARITY,
        UbxCfgKeys::CFG_UART2_STOPBITS
    };
    result = configure(uart2Keys);
    if (!result)
        return false;

    if (base_)
    {
        result = rtkBaseStartup();
        if (!result)
            return false;
    }
    else if (rover_)
    {
        result = rtkRoverStartup();
        if (!result)
            return false;
    }

    return true;
}

bool F9PStartup::rtkBaseStartup()
{
    bool result = false;

    std::vector<uint32_t> tmodeKeys = {
        UbxCfgKeys::CFG_TMODE_MODE
    };

    const auto& baseMode =
        configRegistry_.getGnssConfig().rtk->base->mode;

    if (std::holds_alternative<BaseConfig::SurveyIn>(baseMode))
    {
        tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_SVIN_MIN_DUR);
        tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_SVIN_ACC_LIMIT);
    }
    else if (std::holds_alternative<BaseConfig::FixedPosition>(baseMode))
    {
        const auto& fixed =
            std::get<BaseConfig::FixedPosition>(baseMode);
        tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_POS_TYPE);
        tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_FIXED_POS_ACC);

        if (std::holds_alternative<BaseConfig::FixedPosition::Ecef>(
                fixed.position))
        {
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_ECEF_X);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_ECEF_X_HP);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_ECEF_Y);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_ECEF_Y_HP);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_ECEF_Z);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_ECEF_Z_HP);
        }
        else if (std::holds_alternative<BaseConfig::FixedPosition::Lla>(
                     fixed.position))
        {
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_LAT);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_LAT_HP);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_LON);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_LON_HP);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_HEIGHT);
            tmodeKeys.push_back(UbxCfgKeys::CFG_TMODE_HEIGHT_HP);
        }
    }

    result = configure(tmodeKeys);
    if (!result)
        return false;

    const std::vector<uint32_t> uart2ProtOutKeys = {
        UbxCfgKeys::CFG_UART2OUTPROT_UBX,
        UbxCfgKeys::CFG_UART2OUTPROT_NMEA,
        UbxCfgKeys::CFG_UART2OUTPROT_RTCM3X,
    };
    result = configure(uart2ProtOutKeys);
    if (!result)
        return false;

    const std::vector<uint32_t> rtcm3MsgKeys = {
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1005_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1074_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1077_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1084_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1087_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1094_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1097_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1124_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1127_UART2,
        UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1230_UART2
    };
    result = configure(rtcm3MsgKeys);
    if (!result)
        return false;

    return true;
}

bool F9PStartup::rtkRoverStartup()
{
    bool result = false;

    const std::vector<uint32_t> uart2ProtInKeys = {
        UbxCfgKeys::CFG_UART2INPROT_RTCM3X
    };
    result = configure(uart2ProtInKeys);
    if (!result)
        return false;

    return true;
}

}  // JimmyPaputto
