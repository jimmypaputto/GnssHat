/*
 * Jimmy Paputto 2026
 *
 * TOML configuration loader for gnsshat-rtk-base.
 */

#ifndef GNSSHAT_RTK_BASE_CONFIG_HPP_
#define GNSSHAT_RTK_BASE_CONFIG_HPP_

#include <cstdint>
#include <optional>
#include <string>

#include <toml.hpp>

#include "ublox/BaseConfig.hpp"
#include "ublox/EDynamicModel.hpp"
#include "ublox/ERtkMode.hpp"
#include "ublox/GnssConfig.hpp"
#include "ublox/RtkConfig.hpp"
#include "ublox/TimepulsePinConfig.hpp"
#include "ntrip/NtripLog.hpp"

namespace JimmyPaputto
{

enum class ENtripMode : uint8_t { Caster, Server };
enum class EResetMode : uint8_t { Cold, Hot, None };

struct RtkBaseToolConfig
{
    // ── General ─────────────────────────────────────────────────────
    EResetMode resetMode = EResetMode::Cold;
    uint16_t measurementRate_Hz = 1;
    EDynamicModel dynamicModel = EDynamicModel::Stationary;

    // ── Timepulse ───────────────────────────────────────────────────
    bool timepulseActive = true;
    uint32_t timepulseFrequency_Hz = 1;
    float timepulseDutyCycle = 0.1f;
    std::string timepulsePolarity = "rising";  // "rising" | "falling"

    // ── Base mode ───────────────────────────────────────────────────
    std::string baseMode = "survey_in";  // "survey_in" | "fixed"

    // Survey-in
    uint32_t surveyInMinTime_s = 120;
    double surveyInAccuracy_m = 50.0;

    // Fixed position
    std::string fixedType = "lla";  // "lla" | "ecef"
    double fixedLatitude_deg = 0.0;
    double fixedLongitude_deg = 0.0;
    double fixedHeight_m = 0.0;
    double fixedEcefX_m = 0.0;
    double fixedEcefY_m = 0.0;
    double fixedEcefZ_m = 0.0;
    double fixedAccuracy_m = 0.5;

    // ── NTRIP ───────────────────────────────────────────────────────
    ENtripMode ntripMode = ENtripMode::Caster;
    std::string ntripHost = "0.0.0.0";
    uint16_t ntripPort = 2101;
    std::string ntripMountpoint = "GNSS_HAT";
    std::string ntripUsername;
    std::string ntripPassword;
    size_t ntripMaxClients = 10;
    bool ntripAutoReconnect = true;
    uint32_t ntripReconnectInitialMs = 1000;
    uint32_t ntripReconnectMaxMs = 30000;

    // NTRIP TLS
    bool ntripTlsEnabled = false;
    bool ntripTlsVerifyPeer = true;
    std::string ntripTlsCertFile;
    std::string ntripTlsKeyFile;

    // ── Logging ─────────────────────────────────────────────────────
    ENtripLogLevel ntripLogLevel = ENtripLogLevel::Info;

    // ── Service / logging behavior ──────────────────────────────────
    // serviceMode:
    //   -1 = auto-detect (non-TTY or systemd env vars)
    //    0 = force interactive
    //    1 = force service
    int serviceMode = -1;
    // Main-loop summary interval. 0 = auto (1 s interactive, 30 s service).
    int logIntervalSec = 0;
    // Global log level (also drives NTRIP log level if not otherwise set).
    ENtripLogLevel logLevel = ENtripLogLevel::Info;
    // Honor systemd WATCHDOG_USEC pings when running as a notify service.
    bool watchdogEnabled = true;

    // ── Build GnssConfig from this config ───────────────────────────
    GnssConfig buildGnssConfig() const
    {
        BaseConfig base;
        if (baseMode == "fixed")
        {
            BaseConfig::FixedPosition fp;
            if (fixedType == "ecef")
                fp.position = BaseConfig::FixedPosition::Ecef{fixedEcefX_m, fixedEcefY_m, fixedEcefZ_m};
            else
                fp.position = BaseConfig::FixedPosition::Lla{fixedLatitude_deg, fixedLongitude_deg, fixedHeight_m};
            fp.positionAccuracy_m = fixedAccuracy_m;
            base.mode = fp;
        }
        else
        {
            base.mode = BaseConfig::SurveyIn{surveyInMinTime_s, surveyInAccuracy_m};
        }

        ETimepulsePinPolarity pol = (timepulsePolarity == "falling")
            ? ETimepulsePinPolarity::FallingEdgeAtTopOfSecond
            : ETimepulsePinPolarity::RisingEdgeAtTopOfSecond;

        return GnssConfig{
            .measurementRate_Hz = measurementRate_Hz,
            .dynamicModel = dynamicModel,
            .timepulsePinConfig = TimepulsePinConfig{
                .active = timepulseActive,
                .fixedPulse = TimepulsePinConfig::Pulse{timepulseFrequency_Hz, timepulseDutyCycle},
                .pulseWhenNoFix = std::nullopt,
                .polarity = pol
            },
            .geofencing = std::nullopt,
            .rtk = RtkConfig{
                .mode = ERtkMode::Base,
                .base = base
            }
        };
    }
};

// ── Parse helpers ───────────────────────────────────────────────────

inline EDynamicModel parseDynamicModel(const std::string& s)
{
    if (s == "portable")   return EDynamicModel::Portable;
    if (s == "stationary") return EDynamicModel::Stationary;
    if (s == "pedestrian") return EDynamicModel::Pedestrian;
    if (s == "automotive") return EDynamicModel::Automotive;
    if (s == "sea")        return EDynamicModel::Sea;
    if (s == "airborne1g") return EDynamicModel::Airborne1G;
    if (s == "airborne2g") return EDynamicModel::Airborne2G;
    if (s == "airborne4g") return EDynamicModel::Airborne4G;
    if (s == "wrist")      return EDynamicModel::Wrist;
    if (s == "bike")       return EDynamicModel::Bike;
    if (s == "mower")      return EDynamicModel::Mower;
    if (s == "escooter")   return EDynamicModel::Escooter;
    std::fprintf(stderr, "Warning: unknown dynamic_model '%s', using stationary\n", s.c_str());
    return EDynamicModel::Stationary;
}

inline ENtripLogLevel parseNtripLogLevel(const std::string& s)
{
    if (s == "error")   return ENtripLogLevel::Error;
    if (s == "warning") return ENtripLogLevel::Warning;
    if (s == "info")    return ENtripLogLevel::Info;
    if (s == "debug")   return ENtripLogLevel::Debug;
    return ENtripLogLevel::Info;
}

// ── TOML loader ─────────────────────────────────────────────────────

inline RtkBaseToolConfig loadConfigFromToml(const std::string& path)
{
    RtkBaseToolConfig cfg;
    const auto data = toml::parse(path);

    // Top-level
    if (data.contains("reset_mode"))
    {
        auto v = toml::find<std::string>(data, "reset_mode");
        if (v == "cold")      cfg.resetMode = EResetMode::Cold;
        else if (v == "hot")  cfg.resetMode = EResetMode::Hot;
        else if (v == "none") cfg.resetMode = EResetMode::None;
    }
    if (data.contains("measurement_rate_hz"))
        cfg.measurementRate_Hz = static_cast<uint16_t>(toml::find<int>(data, "measurement_rate_hz"));
    if (data.contains("dynamic_model"))
        cfg.dynamicModel = parseDynamicModel(toml::find<std::string>(data, "dynamic_model"));

    // [timepulse]
    if (data.contains("timepulse"))
    {
        const auto tp = toml::find(data, "timepulse");
        if (tp.contains("active"))
            cfg.timepulseActive = toml::find<bool>(tp, "active");
        if (tp.contains("frequency_hz"))
            cfg.timepulseFrequency_Hz = static_cast<uint32_t>(toml::find<int>(tp, "frequency_hz"));
        if (tp.contains("duty_cycle"))
            cfg.timepulseDutyCycle = static_cast<float>(toml::find<double>(tp, "duty_cycle"));
        if (tp.contains("polarity"))
            cfg.timepulsePolarity = toml::find<std::string>(tp, "polarity");
    }

    // [base]
    if (data.contains("base"))
    {
        const auto base = toml::find(data, "base");
        if (base.contains("mode"))
            cfg.baseMode = toml::find<std::string>(base, "mode");

        // [base.survey_in]
        if (base.contains("survey_in"))
        {
            const auto si = toml::find(base, "survey_in");
            if (si.contains("min_observation_time_s"))
                cfg.surveyInMinTime_s = static_cast<uint32_t>(toml::find<int>(si, "min_observation_time_s"));
            if (si.contains("required_accuracy_m"))
                cfg.surveyInAccuracy_m = toml::find<double>(si, "required_accuracy_m");
        }

        // [base.fixed_position]
        if (base.contains("fixed_position"))
        {
            const auto fp = toml::find(base, "fixed_position");
            if (fp.contains("type"))
                cfg.fixedType = toml::find<std::string>(fp, "type");
            if (fp.contains("latitude_deg"))
                cfg.fixedLatitude_deg = toml::find<double>(fp, "latitude_deg");
            if (fp.contains("longitude_deg"))
                cfg.fixedLongitude_deg = toml::find<double>(fp, "longitude_deg");
            if (fp.contains("height_m"))
                cfg.fixedHeight_m = toml::find<double>(fp, "height_m");
            if (fp.contains("x_m"))
                cfg.fixedEcefX_m = toml::find<double>(fp, "x_m");
            if (fp.contains("y_m"))
                cfg.fixedEcefY_m = toml::find<double>(fp, "y_m");
            if (fp.contains("z_m"))
                cfg.fixedEcefZ_m = toml::find<double>(fp, "z_m");
            if (fp.contains("accuracy_m"))
                cfg.fixedAccuracy_m = toml::find<double>(fp, "accuracy_m");
        }
    }

    // [ntrip]
    if (data.contains("ntrip"))
    {
        const auto ntrip = toml::find(data, "ntrip");
        if (ntrip.contains("mode"))
        {
            auto v = toml::find<std::string>(ntrip, "mode");
            cfg.ntripMode = (v == "server") ? ENtripMode::Server : ENtripMode::Caster;
        }
        if (ntrip.contains("host"))
            cfg.ntripHost = toml::find<std::string>(ntrip, "host");
        if (ntrip.contains("port"))
            cfg.ntripPort = static_cast<uint16_t>(toml::find<int>(ntrip, "port"));
        if (ntrip.contains("mountpoint"))
            cfg.ntripMountpoint = toml::find<std::string>(ntrip, "mountpoint");
        if (ntrip.contains("username"))
            cfg.ntripUsername = toml::find<std::string>(ntrip, "username");
        if (ntrip.contains("password"))
            cfg.ntripPassword = toml::find<std::string>(ntrip, "password");
        if (ntrip.contains("max_clients"))
            cfg.ntripMaxClients = static_cast<size_t>(toml::find<int>(ntrip, "max_clients"));
        if (ntrip.contains("auto_reconnect"))
            cfg.ntripAutoReconnect = toml::find<bool>(ntrip, "auto_reconnect");
        if (ntrip.contains("reconnect_initial_ms"))
            cfg.ntripReconnectInitialMs = static_cast<uint32_t>(toml::find<int>(ntrip, "reconnect_initial_ms"));
        if (ntrip.contains("reconnect_max_ms"))
            cfg.ntripReconnectMaxMs = static_cast<uint32_t>(toml::find<int>(ntrip, "reconnect_max_ms"));

        // [ntrip.tls]
        if (ntrip.contains("tls"))
        {
            const auto tls = toml::find(ntrip, "tls");
            if (tls.contains("enabled"))
                cfg.ntripTlsEnabled = toml::find<bool>(tls, "enabled");
            if (tls.contains("verify_peer"))
                cfg.ntripTlsVerifyPeer = toml::find<bool>(tls, "verify_peer");
            if (tls.contains("cert_file"))
                cfg.ntripTlsCertFile = toml::find<std::string>(tls, "cert_file");
            if (tls.contains("key_file"))
                cfg.ntripTlsKeyFile = toml::find<std::string>(tls, "key_file");
        }
    }

    // [logging]
    if (data.contains("logging"))
    {
        const auto log = toml::find(data, "logging");
        if (log.contains("ntrip_log_level"))
            cfg.ntripLogLevel = parseNtripLogLevel(toml::find<std::string>(log, "ntrip_log_level"));
        if (log.contains("log_level"))
            cfg.logLevel = parseNtripLogLevel(toml::find<std::string>(log, "log_level"));
        if (log.contains("interval_sec"))
            cfg.logIntervalSec = toml::find<int>(log, "interval_sec");
    }

    // [service]
    if (data.contains("service"))
    {
        const auto svc = toml::find(data, "service");
        if (svc.contains("mode"))
        {
            const auto v = toml::find<std::string>(svc, "mode");
            if (v == "auto")             cfg.serviceMode = -1;
            else if (v == "interactive") cfg.serviceMode = 0;
            else if (v == "service")     cfg.serviceMode = 1;
        }
        if (svc.contains("watchdog"))
            cfg.watchdogEnabled = toml::find<bool>(svc, "watchdog");
    }

    return cfg;
}

}  // JimmyPaputto

#endif  // GNSSHAT_RTK_BASE_CONFIG_HPP_
