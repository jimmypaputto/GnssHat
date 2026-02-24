/*
 * Jimmy Paputto 2025
 */

#include "ublox/Startup.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>
#include <variant>

#include "common/Utils.hpp"
#include "ublox/SpiDriver.hpp"
#include "ublox/UartDriver.hpp"
#include "ublox/UbxCfgKeys.hpp"
#include "ublox/ubxmsg/UBX_CFG_CFG.hpp"
#include "ublox/ubxmsg/UBX_CFG_VALGET.hpp"
#include "ublox/ubxmsg/UBX_CFG_VALSET.hpp"


namespace JimmyPaputto
{

StartupBase::StartupBase(ICommDriver& commDriver,
    IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser)
:   commDriver_(commDriver),
    configRegistry_(configRegistry),
    ubxParser_(ubxParser),
    rxBuff_(rxBuffSize)
{
    timepulsePinConfigKeys_.reserve(11);
    timepulsePinConfigKeys_.push_back(UbxCfgKeys::CFG_TP_TP1_ENA);
}

bool StartupBase::saveCurrentConfigToFlash()
{
    bool& ack = configRegistry_.ack()[to_underlying(EUbxMsg::UBX_CFG_CFG)];
    ack = false;
    commDriver_.transmitReceive(ubxmsg::UBX_CFG_CFG::saveToFlash(), rxBuff_);
    ubxParser_.parse(rxBuff_);
    return ack;
}

enum class E_CFG_PULSE_DEF : uint8_t
{
    PERIOD = 0x00,
    FREQ   = 0x01
};

enum class E_CFG_PULSE_LENGTH_DEF : uint8_t
{
    RATIO  = 0x00,
    LENGTH = 0x01
};

enum class E_CFG_TP_TIMEGRID_TP1 : uint8_t
{
    UTC   = 0,
    GPS   = 1,
    GLO   = 2,
    BDS   = 3,
    GAL   = 4,
    NAVIC = 5,
    LOCAL = 15
};

void StartupBase::timepulsePinConfig2Registers(const TimepulsePinConfig& tpc)
{
    using namespace UbxCfgKeys;
    auto& ecv = StartupBase::expectedConfigValues_;

    if (!tpc.active)
    {
        ecv[CFG_TP_TP1_ENA] = {0x00};
        return;
    }

    timepulsePinConfigKeys_.insert(
        timepulsePinConfigKeys_.end(),
        {
            CFG_TP_PULSE_DEF,
            CFG_TP_PULSE_LENGTH_DEF,
            CFG_TP_FREQ_TP1,
            CFG_TP_FREQ_LOCK_TP1,
            CFG_TP_DUTY_TP1,
            CFG_TP_DUTY_LOCK_TP1,
            CFG_TP_ANT_CABLEDELAY,
            CFG_TP_USER_DELAY_TP1,
            CFG_TP_POL_TP1,
            CFG_TP_TIMEGRID_TP1
        }
    );

    ecv[CFG_TP_TP1_ENA] = {0x01};
    ecv[CFG_TP_PULSE_DEF] = {to_underlying(E_CFG_PULSE_DEF::FREQ)};
    ecv[CFG_TP_PULSE_LENGTH_DEF] =
        {to_underlying(E_CFG_PULSE_LENGTH_DEF::RATIO)};
    const uint32_t freqNoFix = tpc.pulseWhenNoFix.has_value() ?
        tpc.pulseWhenNoFix->frequency : 0;
    ecv[CFG_TP_FREQ_TP1] = serializeInt2LittleEndian<uint32_t>(freqNoFix);
    ecv[CFG_TP_FREQ_LOCK_TP1] = serializeInt2LittleEndian<uint32_t>(
        tpc.fixedPulse.frequency
    );
    const double pulseWidthNoFix = tpc.pulseWhenNoFix.has_value() ?
        tpc.pulseWhenNoFix->pulseWidth * 100.0 : 0.0;
    ecv[CFG_TP_DUTY_TP1] = floatingToLittleEndian<double>(pulseWidthNoFix);
    ecv[CFG_TP_DUTY_LOCK_TP1] = floatingToLittleEndian<double>(
        tpc.fixedPulse.pulseWidth * 100.0
    );
    ecv[CFG_TP_ANT_CABLEDELAY] = {0x00, 0x00};
    ecv[CFG_TP_USER_DELAY_TP1] = {0x00, 0x00, 0x00, 0x00};
    ecv[CFG_TP_POL_TP1] = {to_underlying(tpc.polarity)};
    ecv[CFG_TP_TIMEGRID_TP1] = {to_underlying(E_CFG_TP_TIMEGRID_TP1::UTC)};
}

enum class E_CFG_RATE_TIMEREF : uint8_t
{
    UTC   = 0,
    GPS   = 1,
    GLO   = 2,
    BDS   = 3,
    GAL   = 4,
    NAVIC = 5
};

void StartupBase::rate2Registers(const uint16_t measurementRate_Hz)
{
    using namespace UbxCfgKeys;
    auto& ecv = StartupBase::expectedConfigValues_;

    const uint16_t measRate_ms = 1000U / measurementRate_Hz;
    ecv[CFG_RATE_MEAS] = serializeInt2LittleEndian<uint16_t>(measRate_ms);
    const uint16_t navRate = 1;
    ecv[CFG_RATE_NAV] = serializeInt2LittleEndian<uint16_t>(navRate);
    ecv[CFG_RATE_TIMEREF] = {to_underlying(E_CFG_RATE_TIMEREF::UTC)};
}

M9NStartup::M9NStartup(ICommDriver& commDriver,
    IUbloxConfigRegistry& configRegistry, UbxParser& ubxParser)
:	StartupBase(commDriver, configRegistry, ubxParser)
{
    const auto& config = configRegistry.getGnssConfig();
    auto& ecv = StartupBase::expectedConfigValues_;

    ecv[UbxCfgKeys::CFG_NAVSPG_DYNMODEL] = {to_underlying(config.dynamicModel)};
    rate2Registers(config.measurementRate_Hz);
    timepulsePinConfig2Registers(config.timepulsePinConfig);

    constexpr uint32_t fenceUseKeys[4] = {
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE1,
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE2,
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE3,
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE4
    };
    constexpr uint32_t fenceLatKeys[4] = {
        UbxCfgKeys::CFG_GEOFENCE_FENCE1_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE2_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE3_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE4_LAT
    };
    constexpr uint32_t fenceLonKeys[4] = {
        UbxCfgKeys::CFG_GEOFENCE_FENCE1_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE2_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE3_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE4_LON
    };
    constexpr uint32_t fenceRadKeys[4] = {
        UbxCfgKeys::CFG_GEOFENCE_FENCE1_RAD,
        UbxCfgKeys::CFG_GEOFENCE_FENCE2_RAD,
        UbxCfgKeys::CFG_GEOFENCE_FENCE3_RAD,
        UbxCfgKeys::CFG_GEOFENCE_FENCE4_RAD
    };

    const auto& geo = config.geofencing;
    if (geo.has_value())
    {
        ecv[UbxCfgKeys::CFG_GEOFENCE_CONFLVL] = {geo->confidenceLevel};

        if (geo->pioPinPolarity.has_value())
        {
            ecv[UbxCfgKeys::CFG_GEOFENCE_USE_PIO] = {0x01};
            ecv[UbxCfgKeys::CFG_GEOFENCE_PINPOL] =
                {static_cast<uint8_t>(geo->pioPinPolarity.value())};
        }
        else
        {
            ecv[UbxCfgKeys::CFG_GEOFENCE_USE_PIO] = {0x00};
            ecv[UbxCfgKeys::CFG_GEOFENCE_PINPOL] = {0x00};
        }
        ecv[UbxCfgKeys::CFG_GEOFENCE_PIN] = {0x06};  // PIO pin 6

        const auto& fences = geo->geofences;
        const uint8_t numFences =
            static_cast<uint8_t>(std::min(fences.size(),
                static_cast<size_t>(4)));

        for (uint8_t i = 0; i < 4; ++i)
        {
            if (i < numFences)
            {
                ecv[fenceUseKeys[i]] = {0x01};
                const int32_t lat_1e7 =
                    static_cast<int32_t>(fences[i].lat * 1e7);
                const int32_t lon_1e7 =
                    static_cast<int32_t>(fences[i].lon * 1e7);
                const uint32_t rad_cm =
                    static_cast<uint32_t>(fences[i].radius * 100.0f);
                ecv[fenceLatKeys[i]] =
                    serializeInt2LittleEndian<int32_t>(lat_1e7);
                ecv[fenceLonKeys[i]] =
                    serializeInt2LittleEndian<int32_t>(lon_1e7);
                ecv[fenceRadKeys[i]] =
                    serializeInt2LittleEndian<uint32_t>(rad_cm);
            }
            else
            {
                ecv[fenceUseKeys[i]] = {0x00};
                ecv[fenceLatKeys[i]] =
                    serializeInt2LittleEndian<int32_t>(0);
                ecv[fenceLonKeys[i]] =
                    serializeInt2LittleEndian<int32_t>(0);
                ecv[fenceRadKeys[i]] =
                    serializeInt2LittleEndian<uint32_t>(0);
            }
        }
    }
    else
    {
        ecv[UbxCfgKeys::CFG_GEOFENCE_CONFLVL]  = {0x00};
        ecv[UbxCfgKeys::CFG_GEOFENCE_USE_PIO]  = {0x00};
        ecv[UbxCfgKeys::CFG_GEOFENCE_PINPOL]   = {0x00};
        ecv[UbxCfgKeys::CFG_GEOFENCE_PIN]      = {0x06};

        for (uint8_t i = 0; i < 4; ++i)
        {
            ecv[fenceUseKeys[i]] = {0x00};
            ecv[fenceLatKeys[i]] = serializeInt2LittleEndian<int32_t>(0);
            ecv[fenceLonKeys[i]] = serializeInt2LittleEndian<int32_t>(0);
            ecv[fenceRadKeys[i]] = serializeInt2LittleEndian<uint32_t>(0);
        }
    }
}

bool M9NStartup::execute()
{
    bool result = false;

    static constexpr auto flusher = []{
        std::array<uint8_t, 4096> a{};
        for (auto& b : a) b = 0xFF;
        return a;
    }();

    constexpr std::array<uint32_t, 5> spiKeys = {
        UbxCfgKeys::CFG_SPI_MAXFF,
        UbxCfgKeys::CFG_SPI_CPOLARITY,
        UbxCfgKeys::CFG_SPI_CPHASE,
        UbxCfgKeys::CFG_SPI_EXTENDEDTIMEOUT,
        UbxCfgKeys::CFG_SPI_ENABLED
    };
    result = configure(spiKeys);
    if (!result)
    {
        result = reconfigureCommPort();
        if (!result)
            return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 4> spiInProtKeys = {
        UbxCfgKeys::CFG_SPIINPROT_UBX,
        UbxCfgKeys::CFG_SPIINPROT_NMEA,
        UbxCfgKeys::CFG_SPIINPROT_RTCM3X,
        UbxCfgKeys::CFG_SPIINPROT_SPARTN
    };
    result = configure(spiInProtKeys);
    if (!result)
    {
        fprintf(
            stderr,
            "[Startup] SPI input protocol configuration failed\r\n"
        );
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 3> spiOutProtKeys = {
        UbxCfgKeys::CFG_SPIOUTPROT_UBX,
        UbxCfgKeys::CFG_SPIOUTPROT_NMEA,
        UbxCfgKeys::CFG_SPIOUTPROT_RTCM3X
    };
    result = configure(spiOutProtKeys);
    if (!result)
    {
        fprintf(
            stderr,
            "[Startup] SPI output protocol configuration failed\r\n"
        );
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 5> txReadyKeys = {
        UbxCfgKeys::CFG_TXREADY_ENABLED,
        UbxCfgKeys::CFG_TXREADY_POLARITY,
        UbxCfgKeys::CFG_TXREADY_PIN,
        UbxCfgKeys::CFG_TXREADY_THRESHOLD,
        UbxCfgKeys::CFG_TXREADY_INTERFACE
    };
    result = configure(txReadyKeys);
    if (!result)
    {
        fprintf(stderr, "[Startup] TX Ready configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 1> dynModelKeys = {
        UbxCfgKeys::CFG_NAVSPG_DYNMODEL
    };
    result = configure(dynModelKeys);
    if (!result)
    {
        fprintf(stderr, "[Startup] Dynamic model configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 4> geofenceCommonKeys = {
        UbxCfgKeys::CFG_GEOFENCE_CONFLVL,
        UbxCfgKeys::CFG_GEOFENCE_USE_PIO,
        UbxCfgKeys::CFG_GEOFENCE_PINPOL,
        UbxCfgKeys::CFG_GEOFENCE_PIN
    };
    result = configure(geofenceCommonKeys);
    if (!result)
    {
        fprintf(stderr, "[Startup] Geofence common configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 16> geofenceFenceKeys = {
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE1,
        UbxCfgKeys::CFG_GEOFENCE_FENCE1_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE1_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE1_RAD,
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE2,
        UbxCfgKeys::CFG_GEOFENCE_FENCE2_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE2_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE2_RAD,
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE3,
        UbxCfgKeys::CFG_GEOFENCE_FENCE3_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE3_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE3_RAD,
        UbxCfgKeys::CFG_GEOFENCE_USE_FENCE4,
        UbxCfgKeys::CFG_GEOFENCE_FENCE4_LAT,
        UbxCfgKeys::CFG_GEOFENCE_FENCE4_LON,
        UbxCfgKeys::CFG_GEOFENCE_FENCE4_RAD
    };
    result = configure(geofenceFenceKeys);
    if (!result)
    {
        fprintf(stderr, "[Startup] Geofence fences configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 3> rateKeys = {
        UbxCfgKeys::CFG_RATE_MEAS,
        UbxCfgKeys::CFG_RATE_NAV,
        UbxCfgKeys::CFG_RATE_TIMEREF
    };
    result = configure(rateKeys);
    if (!result)
    {
        fprintf(stderr, "[Startup] Rate configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    result = configure(timepulsePinConfigKeys_);
    if (!result)
    {
        fprintf(stderr, "[Startup] Timepulse configuration failed\r\n");
        return false;
    }

    commDriver_.transmitReceive(flusher, rxBuff_);
    commDriver_.transmitReceive(flusher, rxBuff_);

    constexpr std::array<uint32_t, 5> msgoutKeys = {
        UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_SPI,
        UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_SPI,
        UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_SPI,
        UbxCfgKeys::CFG_MSGOUT_UBX_NAV_SAT_SPI,
        UbxCfgKeys::CFG_MSGOUT_UBX_NAV_GEOFENCE_SPI
    };
    result = configure(msgoutKeys);
    if (!result)
    {
        fprintf(stderr, "[Startup] Message output configuration failed\r\n");
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

    constexpr std::array<uint32_t, 5> spiKeys = {
        UbxCfgKeys::CFG_SPI_MAXFF,
        UbxCfgKeys::CFG_SPI_CPOLARITY,
        UbxCfgKeys::CFG_SPI_CPHASE,
        UbxCfgKeys::CFG_SPI_EXTENDEDTIMEOUT,
        UbxCfgKeys::CFG_SPI_ENABLED
    };

    for (const auto& spiMode : spiModesToCheck)
    {
        spiDriver.reinit(spiMode);
        try3times([this, &spiKeys](){ return configure(spiKeys); });
        spiDriver.reinit(SpiDriver::expectedSpiMode);
        result = try3times([this, &spiKeys](){ return configure(spiKeys); });
        if (result)
            break;
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

enum class E_CFG_TXREADY_INTERFACE : uint8_t
{
    I2C = 0x00,
    SPI = 0x01
};

std::unordered_map<uint32_t, std::vector<uint8_t>> StartupBase::expectedConfigValues_ = {
    {UbxCfgKeys::CFG_SPI_MAXFF,           {0x3F}},  // 63
    {UbxCfgKeys::CFG_SPI_CPOLARITY,       {0x00}},
    {UbxCfgKeys::CFG_SPI_CPHASE,          {0x00}},
    {UbxCfgKeys::CFG_SPI_EXTENDEDTIMEOUT, {0x00}},
    {UbxCfgKeys::CFG_SPI_ENABLED,         {0x01}},

    {UbxCfgKeys::CFG_SPIINPROT_UBX,    {0x01}},
    {UbxCfgKeys::CFG_SPIINPROT_NMEA,   {0x00}},
    {UbxCfgKeys::CFG_SPIINPROT_RTCM3X, {0x00}},
    {UbxCfgKeys::CFG_SPIINPROT_SPARTN, {0x00}},

    {UbxCfgKeys::CFG_SPIOUTPROT_UBX,    {0x01}},
    {UbxCfgKeys::CFG_SPIOUTPROT_NMEA,   {0x00}},
    {UbxCfgKeys::CFG_SPIOUTPROT_RTCM3X, {0x00}},

    {UbxCfgKeys::CFG_UART1_ENABLED,  {0x01}},
    {UbxCfgKeys::CFG_UART1_BAUDRATE, {0x00, 0xC2, 0x01, 0x00}},  // 115200
    {UbxCfgKeys::CFG_UART1_DATABITS, {to_underlying(CFG_UART1_DATABITS::EIGHT)}},
    {UbxCfgKeys::CFG_UART1_PARITY,   {to_underlying(CFG_UART1_PARITY::NONE)}},
    {UbxCfgKeys::CFG_UART1_STOPBITS, {to_underlying(CFG_UART1_STOPBITS::ONE)}},

    {UbxCfgKeys::CFG_UART1OUTPROT_UBX,  {0x01}},
    {UbxCfgKeys::CFG_UART1OUTPROT_NMEA, {0x00}},

    {UbxCfgKeys::CFG_UART2_ENABLED,  {0x01}},
    {UbxCfgKeys::CFG_UART2_BAUDRATE, {0x00, 0xC2, 0x01, 0x00}},  // 115200
    {UbxCfgKeys::CFG_UART2_DATABITS, {to_underlying(CFG_UART1_DATABITS::EIGHT)}},
    {UbxCfgKeys::CFG_UART2_PARITY,   {to_underlying(CFG_UART1_PARITY::NONE)}},
    {UbxCfgKeys::CFG_UART2_STOPBITS, {to_underlying(CFG_UART1_STOPBITS::ONE)}},

    {UbxCfgKeys::CFG_UART2INPROT_RTCM3X, {0x01}},

    {UbxCfgKeys::CFG_UART2OUTPROT_UBX,    {0x00}},
    {UbxCfgKeys::CFG_UART2OUTPROT_NMEA,   {0x00}},
    {UbxCfgKeys::CFG_UART2OUTPROT_RTCM3X, {0x01}},

    {UbxCfgKeys::CFG_TXREADY_ENABLED,   {0x01}},
    {UbxCfgKeys::CFG_TXREADY_POLARITY,  {0x00}},
    {UbxCfgKeys::CFG_TXREADY_PIN,       {0x07}},
    {UbxCfgKeys::CFG_TXREADY_THRESHOLD, {0x18, 0x00}},  // 24
    {UbxCfgKeys::CFG_TXREADY_INTERFACE, {to_underlying(E_CFG_TXREADY_INTERFACE::SPI)}},

    {UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_UART1,  {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_UART1, {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_UART1, {0x01}},

    {UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_SPI,       {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_SPI,      {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_SPI,      {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_SAT_SPI,      {0x01}},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_GEOFENCE_SPI, {0x01}},

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
    const auto it = expectedConfigValues_.find(key);
    if (it != expectedConfigValues_.end())
        return it->second;

    return {};
}

bool StartupBase::awaitAck(std::span<const uint8_t> payload, EUbxMsg msgType)
{
    bool& ack = configRegistry_.ack()[to_underlying(msgType)];
    ack = false;
    std::ranges::fill(rxBuff_, 0);
    commDriver_.transmitReceive(payload, rxBuff_);
    ubxParser_.parse(rxBuff_);

    if (ack)
        return true;

    return try3times([this, &ack]() {
        commDriver_.getRxBuff(rxBuff_.data(), rxBuff_.size());
        ubxParser_.parse(rxBuff_);
        return ack;
    });
}

bool StartupBase::verifyConfig(std::span<const uint32_t> keys)
{
    return std::ranges::all_of(keys, [this](const uint32_t key) {
        const auto expected = getExpectedValue(key);
        if (expected.empty())
        {
            fprintf(
                stderr,
                "[Startup] No expected value for key: 0x%08X\r\n",
                key
            );
            std::terminate();
        }
        if (configRegistry_.getStoredConfigValue(key) != expected)
        {
            fprintf(
                stderr,
                "[Startup] Key 0x%08X verification failed\r\n",
                key
            );
            return false;
        }
        return true;
    });
}

bool StartupBase::configure(std::span<const uint32_t> keys)
{
    if (keys.empty())
        return true;

    configRegistry_.clearStoredConfigValues();
    const auto serializedPoll = ubxmsg::UBX_CFG_VALGET::poll(keys);

    if (!awaitAck(serializedPoll, EUbxMsg::UBX_CFG_VALGET))
    {
        fprintf(
            stderr,
            "[Startup] Poll failed for key group: [0x%08X, ...]\r\n",
            keys.front()
        );
        return false;
    }

    std::vector<ubxmsg::ConfigKeyValue> mismatches;
    for (const auto key : keys)
    {
        const auto expected = getExpectedValue(key);
        if (expected.empty())
        {
            fprintf(
                stderr,
                "[Startup] No expected value for keys: [0x%08X, ...]\r\n",
                key
            );
            return false;
        }

        if (configRegistry_.getStoredConfigValue(key) != expected)
        {
            fprintf(
                stderr,
                "[Startup] Key 0x%08X value mismatch\r\n",
                key
            );
            mismatches.push_back({.key = key, .value = expected});
        }
    }

    if (mismatches.empty())
        return true;

    const auto serializedValset = ubxmsg::UBX_CFG_VALSET(
        0x00,
        EUbxMemoryLayer::RAM,
        mismatches
    ).serialize();

    if (!awaitAck(serializedValset, EUbxMsg::UBX_CFG_VALSET))
    {
        fprintf(
            stderr,
            "[Startup] VALSET failed for key group: [0x%08X, ...]\r\n",
            keys.front()
        );
        return false;
    }

    configRegistry_.clearStoredConfigValues();

    if (!awaitAck(serializedPoll, EUbxMsg::UBX_CFG_VALGET))
    {
        fprintf(
            stderr,
            "[Startup] Verification poll failed for key group: [0x%08X, ...]\r\n",
            keys.front()
        );
        return false;
    }

    return verifyConfig(keys);
}

bool F10TStartup::execute()
{
    bool result = false;
    configRegistry_.shouldSaveConfigToFlash(false);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    constexpr std::array<uint32_t, 5> uart1Keys = {
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

    constexpr std::array<uint32_t, 4> txReadyKeys = {
        UbxCfgKeys::CFG_TXREADY_ENABLED,
        UbxCfgKeys::CFG_TXREADY_PIN,
        UbxCfgKeys::CFG_TXREADY_POLARITY,
        UbxCfgKeys::CFG_TXREADY_THRESHOLD,
    };
    result = configure(txReadyKeys);
    if (!result)
        return false;

    constexpr std::array<uint32_t, 2> prtOutProtoKeys = {
        UbxCfgKeys::CFG_UART1OUTPROT_UBX,
        UbxCfgKeys::CFG_UART1OUTPROT_NMEA
    };
    result = configure(prtOutProtoKeys);
    if (!result)
        return false;

    constexpr std::array<uint32_t, 3> msgCfgKeys = {
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

    constexpr std::array<uint32_t, 5> uart1Keys = {
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
            break;
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

    constexpr std::array<uint32_t, 5> uart2Keys = {
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

    constexpr std::array<uint32_t, 3> uart2ProtOutKeys = {
        UbxCfgKeys::CFG_UART2OUTPROT_UBX,
        UbxCfgKeys::CFG_UART2OUTPROT_NMEA,
        UbxCfgKeys::CFG_UART2OUTPROT_RTCM3X,
    };
    result = configure(uart2ProtOutKeys);
    if (!result)
        return false;

    constexpr std::array<uint32_t, 10> rtcm3MsgKeys = {
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

    constexpr std::array<uint32_t, 1> uart2ProtInKeys = {
        UbxCfgKeys::CFG_UART2INPROT_RTCM3X
    };
    result = configure(uart2ProtInKeys);
    if (!result)
        return false;

    return true;
}

}  // JimmyPaputto
