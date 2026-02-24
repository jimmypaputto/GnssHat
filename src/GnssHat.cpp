/*
 * Jimmy Paputto 2024
 */

#include "GnssHat.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <thread>
#include <type_traits>

#include "ublox/Gnss.hpp"
#include "ublox/GnssConfig.hpp"
#include "ublox/NmeaForwarder.hpp"
#include "ublox/RtkFactory.hpp"
#include "ublox/Run.hpp"
#include "ublox/SpiDriver.hpp"
#include "ublox/Timepulse.hpp"
#include "ublox/TxReady.hpp"
#include "ublox/UartDriver.hpp"
#include "ublox/Ublox.hpp"


namespace JimmyPaputto
{

template<typename RunStrategy>
struct RequiresRtcm3Store : std::false_type {};

template<>
struct RequiresRtcm3Store<F9PRun> : std::true_type {};

template<typename RunStrategy>
std::enable_if_t<
    !RequiresRtcm3Store<RunStrategy>::value, 
    std::unique_ptr<IRunStrategy>
> createRunStrategy(
    ICommDriver& commDriver, 
    UbxParser& ubxParser, 
    Notifier& txReadyNotifier, 
    Notifier& navigationNotifier)
{
    if constexpr (std::is_same_v<RunStrategy, M9NRun>)
    {
        return std::make_unique<M9NRun>(
            commDriver, ubxParser, txReadyNotifier, navigationNotifier
        );
    }
    else if constexpr (std::is_same_v<RunStrategy, F10TRun>)
    {
        return std::make_unique<F10TRun>(commDriver, ubxParser);
    }
}

template<typename RunStrategy>
std::enable_if_t<
    RequiresRtcm3Store<RunStrategy>::value, 
    std::unique_ptr<IRunStrategy>
> createRunStrategy(
    ICommDriver& commDriver, 
    UbxParser& ubxParser, 
    Notifier& txReadyNotifier, 
    Notifier& navigationNotifier,
    Rtcm3Store& rtcm3Store,
    const GnssConfig& config)
{
    return std::make_unique<F9PRun>(
        commDriver, ubxParser, txReadyNotifier, navigationNotifier, rtcm3Store,
        config
    );
}

class GnssHat : public IGnssHat
{
public:
    explicit GnssHat(
        std::unique_ptr<ICommDriver>&& commDriver
    );
    ~GnssHat() override;

    template<class StartupStrategy, class RunStrategy>
    bool start(const GnssConfig& config);
    Navigation waitAndGetFreshNavigation() override;
    Navigation navigation() const override;
    bool enableTimepulse() override;
    void disableTimepulse() override;
    bool startForwardForGpsd() override;
    void stopForwardForGpsd() override;
    void joinForwardForGpsd() override;
    std::string getGpsdDevicePath() const override;
    void hardResetUbloxSom_ColdStart() const override;
    void softResetUbloxSom_HotStart() override;
    void timepulse() override;

protected:
    void stopUbloxThread();
    virtual std::optional<std::reference_wrapper<Rtcm3Store>> rtcm3Store();

    std::unique_ptr<ICommDriver> commDriver_;
    std::unique_ptr<IUbloxConfigRegistry> configRegistry_;
    std::unique_ptr<UbxParser> ubxParser_;
    std::unique_ptr<IRunStrategy> runStrategy_;
    std::unique_ptr<IStartupStrategy> startupStrategy_;
    std::unique_ptr<Ublox> ublox_;
    Gnss& gnss_;
    std::unique_ptr<TxReadyInterrupt> txReady_;
    std::unique_ptr<Timepulse> timepulse_;
    std::unique_ptr<NmeaForwarder> nmeaForwarder_;
    Notifier txReadyNotifier_;
    Notifier timepulseNotifier_;
    Notifier navigationNotifier_;
    GnssConfig config_;

    std::jthread ubloxThread_;
    std::atomic<bool> timepulseEnabled_{false};
};

class GnssL1Hat : public GnssHat
{
public:
    explicit GnssL1Hat()
    :   GnssHat(
            std::make_unique<SpiDriver>()
        )
    {}

    ~GnssL1Hat() override = default;

    bool start(const GnssConfig& config) override
    {
        return GnssHat::start<M9NStartup, M9NRun>(config);
    }

    IRtk* rtk() override
    {
        return nullptr;
    }
};

class GnssL1L5TimeHat : public GnssHat
{
public:
    explicit GnssL1L5TimeHat()
    :   GnssHat(
            std::make_unique<UartDriver>()
        )
    {}

    ~GnssL1L5TimeHat() override = default;

    bool start(const GnssConfig& config) override
    {
        return GnssHat::start<F10TStartup, F10TRun>(config);
    }

    IRtk* rtk() override
    {
        return nullptr;
    }
};

class GnssL1L5TRtkHat : public GnssHat
{
public:
    explicit GnssL1L5TRtkHat()
    :   GnssHat(
            std::make_unique<SpiDriver>()
        ),
        rtk_(nullptr)
    {}

    ~GnssL1L5TRtkHat() override = default;

    bool start(const GnssConfig& config) override
    {
        rtk_ = std::unique_ptr<IRtk>(RtkFactory::create(rtcm3Store_, config));
        return GnssHat::start<F9PStartup, F9PRun>(config);
    }

    IRtk* rtk() override
    {
        return rtk_.get();
    }

    std::optional<std::reference_wrapper<Rtcm3Store>> rtcm3Store() override
    {
        return std::ref(rtcm3Store_);
    }

private:
    Rtcm3Store rtcm3Store_;
    std::unique_ptr<IRtk> rtk_;
};

static std::string readHatProduct()
{
    const std::string productPath = "/proc/device-tree/hat/product";
    std::ifstream file(productPath);
    if (!file.is_open())
        return "";

    std::string product;
    std::getline(file, product, '\0');
    return product;
}

IGnssHat* IGnssHat::create()
{
    const std::string GnssL1HatProduct = "L1 GNSS HAT";
    const std::string GnssL1L5TimeHatProduct = "L1/L5 GNSS TIME HAT";
    const std::string GnssL1L5RtkHatProduct = "L1/L5 GNSS RTK HAT";
    const std::string hatProduct = readHatProduct();
 
    if (GnssL1HatProduct == hatProduct)
        return new GnssL1Hat();
    if (GnssL1L5TimeHatProduct == hatProduct)
        return new GnssL1L5TimeHat();
    if (GnssL1L5RtkHatProduct == hatProduct)
        return new GnssL1L5TRtkHat();

    printf("[GNSS] Unknown HAT, FATAL\r\n");
    std::terminate();
}

GnssHat::GnssHat(std::unique_ptr<ICommDriver>&& commDriver)
:   commDriver_(std::move(commDriver)),
    configRegistry_(nullptr),
    ubxParser_(nullptr),
    startupStrategy_(nullptr),
    ublox_(nullptr),
    gnss_(Gnss::instance()),
    txReady_(nullptr),
    timepulse_(nullptr),
    nmeaForwarder_(nullptr)
{
}

GnssHat::~GnssHat()
{
    stopUbloxThread();
}

template<class StartupStrategy>
bool validateConfig(const GnssConfig& config)
{
    if (!checkMeasurmentRate(config.measurementRate_Hz))
        return false;
    if (!checkTimepulsePinConfig(config.timepulsePinConfig))
        return false;

    if constexpr (std::is_same_v<StartupStrategy, M9NStartup> ||
        std::is_same_v<StartupStrategy, F9PStartup>)
    {
        return checkGeofencing(config.geofencing);
    }
    else if constexpr (std::is_same_v<StartupStrategy, F10TStartup>)
    {
        if (config.geofencing.has_value())
        {
            fprintf(
                stderr,
                "[GnssConfig] F10T does not support geofencing - "
                "must be nullopt\r\n"
            );
            return false;
        }
        return true;
    }

    return false;
}

template<class StartupStrategy, class RunStrategy>
bool GnssHat::start(const GnssConfig& config)
{
    if (!validateConfig<StartupStrategy>(config))
    {
        fprintf(stderr, "[GNSS] Invalid config\r\n");
        return false;
    }

    config_ = config;
    configRegistry_ = std::make_unique<UbloxConfigRegistry>(config_);
    constexpr bool callbackNotificationEnabled =
        std::is_same_v<RunStrategy, F10TRun>;
    ubxParser_ = std::make_unique<UbxParser>(
        *configRegistry_, navigationNotifier_, callbackNotificationEnabled
    );
    startupStrategy_ = std::make_unique<StartupStrategy>(
        *commDriver_, *configRegistry_, *ubxParser_
    );
    if constexpr (std::is_same_v<RunStrategy, F9PRun>)
    {
        runStrategy_ = createRunStrategy<RunStrategy>(
            *commDriver_, *ubxParser_, txReadyNotifier_, navigationNotifier_,
            rtcm3Store()->get(), config
        );
    }
    else
    {
        runStrategy_ = createRunStrategy<RunStrategy>(
            *commDriver_, *ubxParser_, txReadyNotifier_, navigationNotifier_
        );
    }
    ublox_ = std::make_unique<Ublox>(
        *commDriver_, *configRegistry_, *ubxParser_, *startupStrategy_,
        *runStrategy_, navigationNotifier_
    );

    const bool isStartupDone = ublox_->startup();
    if (!isStartupDone)
    {
        fprintf(stderr, "[GNSS] Startup failed, check your hat\r\n");
        return false;
    }

    if constexpr (!std::is_same_v<RunStrategy, F10TRun>)
    {
        txReady_ = std::make_unique<TxReadyInterrupt>(
            txReadyNotifier_, config.measurementRate_Hz
        );
        txReady_->run();
    }

    ubloxThread_ = std::jthread([this](std::stop_token stoken){
        while (!stoken.stop_requested())
        {
            ublox_->run();
        }
    });

    return true;
}

Navigation GnssHat::waitAndGetFreshNavigation()
{
    navigationNotifier_.wait();

    Navigation navigation;
    if (gnss_.lock())
    {
        navigation = gnss_.navigation();
        gnss_.unlock();
    }
    return navigation;
}

Navigation GnssHat::navigation() const
{
    Navigation navigation;
    if (gnss_.lock())
    {
        navigation = gnss_.navigation();
        gnss_.unlock();
    }
    return navigation;
}

bool GnssHat::enableTimepulse()
{
    if (timepulseEnabled_.load())
    {
        fprintf(stderr, "[GNSS] Timepulse already enabled\r\n");
        return true;
    }

    try
    {
        timepulse_ = std::make_unique<Timepulse>(timepulseNotifier_);
        timepulse_->run();
        timepulseEnabled_.store(true);
        return true;
    }
    catch (const std::exception& e)
    {
        fprintf(
            stderr,
            "[GNSS] Failed to enable timepulse: %s\r\n",
            e.what()
        );
        return false;
    }
    catch (...)
    {
        fprintf(
            stderr,
            "[GNSS] Failed to enable timepulse: Unknown error\r\n"
        );
        return false;
    }
}

void GnssHat::disableTimepulse()
{
    if (!timepulseEnabled_.load())
        return;

    timepulse_.reset();
    timepulseEnabled_.store(false);
}

void GnssHat::hardResetUbloxSom_ColdStart() const
{
    Ublox::powerOffUbloxSom();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Ublox::powerOnUbloxSom();
}

void GnssHat::softResetUbloxSom_HotStart()
{
    static constexpr std::array<uint8_t, 12> txBuffer = {
        0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x66
    };
    std::vector<uint8_t> rxBuffer(txBuffer.size());
    commDriver_->transmitReceive(txBuffer, rxBuffer);
}

void GnssHat::timepulse()
{
    if (!timepulseEnabled_.load() || !timepulse_)
    {
        fprintf(
            stderr,
            "[GNSS] Timepulse not enabled. Call enableTimepulse() first.\r\n"
        );
        return;
    }
    timepulseNotifier_.wait();
}

bool GnssHat::startForwardForGpsd()
{
    if (nmeaForwarder_ && nmeaForwarder_->isRunning())
    {
        fprintf(stderr, "[GNSS] NMEA forwarding already active\r\n");
        return true;
    }

    nmeaForwarder_ = std::make_unique<NmeaForwarder>();

    if (!nmeaForwarder_->createVirtualTty())
    {
        fprintf(stderr, "[GNSS] Failed to create virtual TTY for GPSD\r\n");
        nmeaForwarder_.reset();
        return false;
    }

    nmeaForwarder_->startForwarding(gnss_);
    return true;
}

void GnssHat::stopForwardForGpsd()
{
    if (nmeaForwarder_)
    {
        nmeaForwarder_->stopForwarding();
        nmeaForwarder_->joinForwarding();
        nmeaForwarder_.reset();
        printf("[GNSS] GPSD forwarding stopped\n");
    }
}

void GnssHat::joinForwardForGpsd()
{
    if (!nmeaForwarder_ || !nmeaForwarder_->isRunning())
        return;

    nmeaForwarder_->joinForwarding();
}

std::string GnssHat::getGpsdDevicePath() const
{
    if (nmeaForwarder_)
        return nmeaForwarder_->getDevicePath();
    return "";
}

void GnssHat::stopUbloxThread()
{
    ubloxThread_.request_stop();
    if (ubloxThread_.joinable())
        ubloxThread_.join();
}

std::optional<std::reference_wrapper<Rtcm3Store>> GnssHat::rtcm3Store()
{
    return std::nullopt;
}

namespace Utils
{

std::string eFixQuality2string(const EFixQuality e)
{
    switch (e)
    {
    case EFixQuality::Invalid:
        return "Invalid";
    case EFixQuality::GpsFix2D3D:
        return "GpsFix2D3D";
    case EFixQuality::DGNSS:
        return "DGNSS";
    case EFixQuality::PpsFix:
        return "PpsFix";
    case EFixQuality::FixedRTK:
        return "FixedRTK";
    case EFixQuality::FloatRtk:
        return "FloatRtk";
    case EFixQuality::DeadReckoning:
        return "DeadReckoning";
    default:
        return "Unknown";
    }
}

std::string eFixStatus2string(const EFixStatus e)
{
    switch (e)
    {
    case EFixStatus::Void:
        return "Void";
    case EFixStatus::Active:
        return "Active";
    default:
        return "Unknown";
    }
}

std::string eFixType2string(const EFixType e)
{
    switch (e)
    {
    case EFixType::NoFix:
        return "NoFix";
    case EFixType::DeadReckoningOnly:
        return "DeadReckoningOnly";
    case EFixType::Fix2D:
        return "Fix2D";
    case EFixType::Fix3D:
        return "Fix3D";
    case EFixType::GnssWithDeadReckoning:
        return "GnssWithDeadReckoning";
    case EFixType::TimeOnlyFix:
        return "TimeOnlyFix";
    default:
        return "Unknown";
    }
}

std::string jammingState2string(const EJammingState e)
{
    switch (e)
    {
    case EJammingState::Unknown:
        return "Unknown";
    case EJammingState::Ok_NoSignifantJamming:
        return "Ok_NoSignifantJamming";
    case EJammingState::Warning_InferenceVisibleButFixOk:
        return "Warning_InferenceVisibleButFixOk";
    case EJammingState::Critical_InferenceVisibleAndNoFix:
        return "Critical_InferenceVisibleAndNoFix";
    default:
        return "Unknown";
    }
}

std::string antennaStatus2string(const EAntennaStatus e)
{
    switch (e)
    {
    case EAntennaStatus::Init:
        return "Init";
    case EAntennaStatus::DontKnow:
        return "DontKnow";
    case EAntennaStatus::Ok:
        return "Ok";
    case EAntennaStatus::Short:
        return "Short";
    case EAntennaStatus::Open:
        return "Open";
    default:
        return "Unknown";
    }
}

std::string antennaPower2string(const EAntennaPower e)
{
    switch (e)
    {
    case EAntennaPower::Off:
        return "Off";
    case EAntennaPower::On:
        return "On";
    case EAntennaPower::DontKnow:
        return "DontKnow";
    default:
        return "Unknown";
    }
}

std::string eBand2string(const EBand e)
{
    switch (e)
    {
    case EBand::L1:
        return "L1";
    case EBand::L2orL5:
        return "L2 or L5";
    default:
        return "Unknown";
    }
}

std::string geofencingStatus2string(const EGeofencingStatus e)
{
    switch (e)
    {
    case EGeofencingStatus::NotAvalaible:
        return "NotAvailable";
    case EGeofencingStatus::Active:
        return "Active";
    default:
        return "Unknown";
    }
}

std::string geofenceStatus2string(const EGeofenceStatus e)
{
    switch (e)
    {
    case EGeofenceStatus::Unknown:
        return "Unknown";
    case EGeofenceStatus::Inside:
        return "Inside";
    case EGeofenceStatus::Outside:
        return "Outside";
    default:
        return "Unknown";
    }
}

std::string utcTimeFromGnss_ISO8601(const PositionVelocityTime& pvt)
{
    char buffer[32];
    snprintf(
        buffer, 
        sizeof(buffer),
        "%04d-%02d-%02dT%02d:%02d:%02dZ",
        pvt.date.year,
        pvt.date.month,
        pvt.date.day,
        pvt.utc.hh,
        pvt.utc.mm,
        pvt.utc.ss
    );
    return std::string(buffer);
}

}  // Utils

}  // JimmyPaputto
