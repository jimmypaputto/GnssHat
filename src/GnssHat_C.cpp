/*
 * Jimmy Paputto 2025
 */

#include "GnssHat.h"
#include "GnssHat.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <new>
#include <optional>


using namespace JimmyPaputto;

// Static asserts to ensure C and C++ enum values stay in sync
static_assert(static_cast<int>(EDynamicModel::Portable) == JP_GNSS_DYNAMIC_MODEL_PORTABLE);
static_assert(static_cast<int>(EDynamicModel::Stationary) == JP_GNSS_DYNAMIC_MODEL_STATIONARY);
static_assert(static_cast<int>(EDynamicModel::Pedestrian) == JP_GNSS_DYNAMIC_MODEL_PEDESTRIAN);
static_assert(static_cast<int>(EDynamicModel::Automotive) == JP_GNSS_DYNAMIC_MODEL_AUTOMOTIVE);
static_assert(static_cast<int>(EDynamicModel::Sea) == JP_GNSS_DYNAMIC_MODEL_SEA);
static_assert(static_cast<int>(EDynamicModel::Airborne1G) == JP_GNSS_DYNAMIC_MODEL_AIRBORNE_1G);
static_assert(static_cast<int>(EDynamicModel::Airborne2G) == JP_GNSS_DYNAMIC_MODEL_AIRBORNE_2G);
static_assert(static_cast<int>(EDynamicModel::Airborne4G) == JP_GNSS_DYNAMIC_MODEL_AIRBORNE_4G);
static_assert(static_cast<int>(EDynamicModel::Wrist) == JP_GNSS_DYNAMIC_MODEL_WRIST);
static_assert(static_cast<int>(EDynamicModel::Bike) == JP_GNSS_DYNAMIC_MODEL_BIKE);
static_assert(static_cast<int>(EDynamicModel::Mower) == JP_GNSS_DYNAMIC_MODEL_MOWER);
static_assert(static_cast<int>(EDynamicModel::Escooter) == JP_GNSS_DYNAMIC_MODEL_ESCOOTER);

static_assert(static_cast<int>(EFixQuality::Invalid) == JP_GNSS_FIX_QUALITY_INVALID);
static_assert(static_cast<int>(EFixQuality::GpsFix2D3D) == JP_GNSS_FIX_QUALITY_GPS_FIX_2D_3D);
static_assert(static_cast<int>(EFixQuality::DGNSS) == JP_GNSS_FIX_QUALITY_DGNSS);
static_assert(static_cast<int>(EFixQuality::PpsFix) == JP_GNSS_FIX_QUALITY_PPS_FIX);
static_assert(static_cast<int>(EFixQuality::FixedRTK) == JP_GNSS_FIX_QUALITY_FIXED_RTK);
static_assert(static_cast<int>(EFixQuality::FloatRtk) == JP_GNSS_FIX_QUALITY_FLOAT_RTK);
static_assert(static_cast<int>(EFixQuality::DeadReckoning) == JP_GNSS_FIX_QUALITY_DEAD_RECKONING);

static_assert(static_cast<int>(EFixStatus::Void) == JP_GNSS_FIX_STATUS_VOID);
static_assert(static_cast<int>(EFixStatus::Active) == JP_GNSS_FIX_STATUS_ACTIVE);

static_assert(static_cast<int>(EFixType::NoFix) == JP_GNSS_FIX_TYPE_NO_FIX);
static_assert(static_cast<int>(EFixType::DeadReckoningOnly) == JP_GNSS_FIX_TYPE_DEAD_RECKONING_ONLY);
static_assert(static_cast<int>(EFixType::Fix2D) == JP_GNSS_FIX_TYPE_FIX_2D);
static_assert(static_cast<int>(EFixType::Fix3D) == JP_GNSS_FIX_TYPE_FIX_3D);
static_assert(static_cast<int>(EFixType::GnssWithDeadReckoning) == JP_GNSS_FIX_TYPE_GNSS_WITH_DEAD_RECKONING);
static_assert(static_cast<int>(EFixType::TimeOnlyFix) == JP_GNSS_FIX_TYPE_TIME_ONLY_FIX);

static_assert(static_cast<int>(ETimepulsePinPolarity::FallingEdgeAtTopOfSecond) == JP_GNSS_TIMEPULSE_POLARITY_FALLING_EDGE);
static_assert(static_cast<int>(ETimepulsePinPolarity::RisingEdgeAtTopOfSecond) == JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE);

static_assert(static_cast<int>(EPioPinPolarity::LowMeansInside) == JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_INSIDE);
static_assert(static_cast<int>(EPioPinPolarity::LowMeansOutside) == JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_OUTSIDE);

static_assert(static_cast<int>(EGeofenceStatus::Unknown) == JP_GNSS_GEOFENCE_STATUS_UNKNOWN);
static_assert(static_cast<int>(EGeofenceStatus::Inside) == JP_GNSS_GEOFENCE_STATUS_INSIDE);
static_assert(static_cast<int>(EGeofenceStatus::Outside) == JP_GNSS_GEOFENCE_STATUS_OUTSIDE);

static_assert(static_cast<int>(EGeofencingStatus::NotAvailable) == JP_GNSS_GEOFENCING_STATUS_NOT_AVAILABLE);
static_assert(static_cast<int>(EGeofencingStatus::Active) == JP_GNSS_GEOFENCING_STATUS_ACTIVE);

static_assert(static_cast<int>(EGnssBand::UNKNOWN) == JP_GNSS_RF_BAND_UNKNOWN);
static_assert(static_cast<int>(EGnssBand::L1) == JP_GNSS_RF_BAND_L1);
static_assert(static_cast<int>(EGnssBand::L2) == JP_GNSS_RF_BAND_L2);
static_assert(static_cast<int>(EGnssBand::L3) == JP_GNSS_RF_BAND_L3);
static_assert(static_cast<int>(EGnssBand::L5) == JP_GNSS_RF_BAND_L5);
static_assert(static_cast<int>(EGnssBand::L2orL5) == JP_GNSS_RF_BAND_L2_OR_L5);
static_assert(static_cast<int>(EJammingState::Unknown) == JP_GNSS_JAMMING_STATE_UNKNOWN);
static_assert(static_cast<int>(EJammingState::Ok_NoSignificantJamming) == JP_GNSS_JAMMING_STATE_OK_NO_SIGNIFICANT_JAMMING);
static_assert(static_cast<int>(EJammingState::Warning_InterferenceVisibleButFixOk) == JP_GNSS_JAMMING_STATE_WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK);
static_assert(static_cast<int>(EJammingState::Critical_InterferenceVisibleAndNoFix) == JP_GNSS_JAMMING_STATE_CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX);

static_assert(static_cast<int>(EAntennaStatus::Init) == JP_GNSS_ANTENNA_STATUS_INIT);
static_assert(static_cast<int>(EAntennaStatus::DontKnow) == JP_GNSS_ANTENNA_STATUS_DONT_KNOW);
static_assert(static_cast<int>(EAntennaStatus::Ok) == JP_GNSS_ANTENNA_STATUS_OK);
static_assert(static_cast<int>(EAntennaStatus::Short) == JP_GNSS_ANTENNA_STATUS_SHORT);
static_assert(static_cast<int>(EAntennaStatus::Open) == JP_GNSS_ANTENNA_STATUS_OPEN);

static_assert(static_cast<int>(EAntennaPower::Off) == JP_GNSS_ANTENNA_POWER_OFF);
static_assert(static_cast<int>(EAntennaPower::On) == JP_GNSS_ANTENNA_POWER_ON);
static_assert(static_cast<int>(EAntennaPower::DontKnow) == JP_GNSS_ANTENNA_POWER_DONT_KNOW);

static_assert(static_cast<int>(EGnssId::GPS) == JP_GNSS_GNSS_ID_GPS);
static_assert(static_cast<int>(EGnssId::SBAS) == JP_GNSS_GNSS_ID_SBAS);
static_assert(static_cast<int>(EGnssId::Galileo) == JP_GNSS_GNSS_ID_GALILEO);
static_assert(static_cast<int>(EGnssId::BeiDou) == JP_GNSS_GNSS_ID_BEIDOU);
static_assert(static_cast<int>(EGnssId::IMES) == JP_GNSS_GNSS_ID_IMES);
static_assert(static_cast<int>(EGnssId::QZSS) == JP_GNSS_GNSS_ID_QZSS);
static_assert(static_cast<int>(EGnssId::GLONASS) == JP_GNSS_GNSS_ID_GLONASS);

static_assert(static_cast<int>(ESvQuality::NoSignal) == JP_GNSS_SV_QUALITY_NO_SIGNAL);
static_assert(static_cast<int>(ESvQuality::Searching) == JP_GNSS_SV_QUALITY_SEARCHING);
static_assert(static_cast<int>(ESvQuality::SignalAcquired) == JP_GNSS_SV_QUALITY_SIGNAL_ACQUIRED);
static_assert(static_cast<int>(ESvQuality::SignalDetectedButUnusable) == JP_GNSS_SV_QUALITY_SIGNAL_DETECTED_BUT_UNUSABLE);
static_assert(static_cast<int>(ESvQuality::CodeLockedAndTimeSynchronized) == JP_GNSS_SV_QUALITY_CODE_LOCKED_AND_TIME_SYNCHRONIZED);
static_assert(static_cast<int>(ESvQuality::CodeAndCarrierLocked1) == JP_GNSS_SV_QUALITY_CODE_AND_CARRIER_LOCKED_1);
static_assert(static_cast<int>(ESvQuality::CodeAndCarrierLocked2) == JP_GNSS_SV_QUALITY_CODE_AND_CARRIER_LOCKED_2);
static_assert(static_cast<int>(ESvQuality::CodeAndCarrierLocked3) == JP_GNSS_SV_QUALITY_CODE_AND_CARRIER_LOCKED_3);

static_assert(SatelliteInfo::maxNumberOfSatellites == UBLOX_MAX_SATELLITES);

static_assert(static_cast<int>(EBootType::Unknown)        == JP_GNSS_BOOT_TYPE_UNKNOWN);
static_assert(static_cast<int>(EBootType::ColdStart)      == JP_GNSS_BOOT_TYPE_COLD_START);
static_assert(static_cast<int>(EBootType::Watchdog)       == JP_GNSS_BOOT_TYPE_WATCHDOG);
static_assert(static_cast<int>(EBootType::HardwareReset)  == JP_GNSS_BOOT_TYPE_HARDWARE_RESET);
static_assert(static_cast<int>(EBootType::HardwareBackup) == JP_GNSS_BOOT_TYPE_HARDWARE_BACKUP);
static_assert(static_cast<int>(EBootType::SoftwareBackup) == JP_GNSS_BOOT_TYPE_SOFTWARE_BACKUP);
static_assert(static_cast<int>(EBootType::SoftwareReset)  == JP_GNSS_BOOT_TYPE_SOFTWARE_RESET);
static_assert(static_cast<int>(EBootType::VioFail)        == JP_GNSS_BOOT_TYPE_VIO_FAIL);
static_assert(static_cast<int>(EBootType::VddXFail)       == JP_GNSS_BOOT_TYPE_VDD_X_FAIL);
static_assert(static_cast<int>(EBootType::VddRfFail)      == JP_GNSS_BOOT_TYPE_VDD_RF_FAIL);
static_assert(static_cast<int>(EBootType::VCoreHighFail)  == JP_GNSS_BOOT_TYPE_V_CORE_HIGH_FAIL);
static_assert(static_cast<int>(EBootType::SystemReset)    == JP_GNSS_BOOT_TYPE_SYSTEM_RESET);

static_assert(static_cast<int>(ERtkMode::Base) == JP_GNSS_RTK_MODE_BASE);
static_assert(static_cast<int>(ERtkMode::Rover) == JP_GNSS_RTK_MODE_ROVER);

static_assert(static_cast<int>(ETimeMarkMode::Single) == JP_GNSS_TIME_MARK_MODE_SINGLE);
static_assert(static_cast<int>(ETimeMarkMode::Running) == JP_GNSS_TIME_MARK_MODE_RUNNING);

static_assert(static_cast<int>(ETimeMarkRun::Armed) == JP_GNSS_TIME_MARK_RUN_ARMED);
static_assert(static_cast<int>(ETimeMarkRun::Stopped) == JP_GNSS_TIME_MARK_RUN_STOPPED);

static_assert(static_cast<int>(ETimeMarkTimeBase::ReceiverTime) == JP_GNSS_TIME_MARK_TIME_BASE_RECEIVER);
static_assert(static_cast<int>(ETimeMarkTimeBase::GnssTime) == JP_GNSS_TIME_MARK_TIME_BASE_GNSS);
static_assert(static_cast<int>(ETimeMarkTimeBase::UTC) == JP_GNSS_TIME_MARK_TIME_BASE_UTC);

static_assert(static_cast<int>(ETimeMarkTriggerEdge::Rising) == JP_GNSS_TIME_MARK_TRIGGER_EDGE_RISING);
static_assert(static_cast<int>(ETimeMarkTriggerEdge::Falling) == JP_GNSS_TIME_MARK_TRIGGER_EDGE_FALLING);
static_assert(static_cast<int>(ETimeMarkTriggerEdge::Toggle) == JP_GNSS_TIME_MARK_TRIGGER_EDGE_TOGGLE);

namespace
{

EDynamicModel convert_dynamic_model(jp_gnss_dynamic_model_t model)
{
    return static_cast<EDynamicModel>(model);
}

ETimepulsePinPolarity convert_timepulse_polarity(
    jp_gnss_timepulse_polarity_t polarity)
{
    return static_cast<ETimepulsePinPolarity>(polarity);
}

EPioPinPolarity convert_pio_pin_polarity(jp_gnss_pio_pin_polarity_t polarity)
{
    return static_cast<EPioPinPolarity>(polarity);
}

jp_gnss_pio_pin_polarity_t convert_pio_pin_polarity(EPioPinPolarity polarity)
{
    return static_cast<jp_gnss_pio_pin_polarity_t>(polarity);
}

EFixQuality convert_fix_quality(jp_gnss_fix_quality_t quality)
{
    return static_cast<EFixQuality>(quality);
}

jp_gnss_fix_quality_t convert_fix_quality(EFixQuality quality)
{
    return static_cast<jp_gnss_fix_quality_t>(quality);
}

EFixStatus convert_fix_status(jp_gnss_fix_status_t status)
{
    return static_cast<EFixStatus>(status);
}

jp_gnss_fix_status_t convert_fix_status(EFixStatus status)
{
    return static_cast<jp_gnss_fix_status_t>(status);
}

EFixType convert_fix_type(jp_gnss_fix_type_t type)
{
    return static_cast<EFixType>(type);
}

jp_gnss_fix_type_t convert_fix_type(EFixType type)
{
    return static_cast<jp_gnss_fix_type_t>(type);
}

EGeofenceStatus convert_geofence_status(jp_gnss_geofence_status_t status)
{
    return static_cast<EGeofenceStatus>(status);
}

jp_gnss_geofence_status_t convert_geofence_status(EGeofenceStatus status)
{
    return static_cast<jp_gnss_geofence_status_t>(status);
}

EGeofencingStatus convert_geofencing_status(jp_gnss_geofencing_status_t status)
{
    return static_cast<EGeofencingStatus>(status);
}

jp_gnss_geofencing_status_t convert_geofencing_status(EGeofencingStatus status)
{
    return static_cast<jp_gnss_geofencing_status_t>(status);
}

EGnssBand convert_rf_band(jp_gnss_rf_band_t band)
{
    return static_cast<EGnssBand>(band);
}

jp_gnss_rf_band_t convert_rf_band(EGnssBand band)
{
    return static_cast<jp_gnss_rf_band_t>(band);
}

EJammingState convert_jamming_state(jp_gnss_jamming_state_t state)
{
    return static_cast<EJammingState>(state);
}

jp_gnss_jamming_state_t convert_jamming_state(EJammingState state)
{
    return static_cast<jp_gnss_jamming_state_t>(state);
}

EAntennaStatus convert_antenna_status(jp_gnss_antenna_status_t status)
{
    return static_cast<EAntennaStatus>(status);
}

jp_gnss_antenna_status_t convert_antenna_status(EAntennaStatus status)
{
    return static_cast<jp_gnss_antenna_status_t>(status);
}

EAntennaPower convert_antenna_power(jp_gnss_antenna_power_t power)
{
    return static_cast<EAntennaPower>(power);
}

jp_gnss_antenna_power_t convert_antenna_power(EAntennaPower power)
{
    return static_cast<jp_gnss_antenna_power_t>(power);
}

jp_gnss_gnss_id_t convert_gnss_id(EGnssId id)
{
    return static_cast<jp_gnss_gnss_id_t>(id);
}

jp_gnss_sv_quality_t convert_sv_quality(ESvQuality quality)
{
    return static_cast<jp_gnss_sv_quality_t>(quality);
}

TimepulsePinConfig convert_timepulse_config(
    const jp_gnss_timepulse_pin_config_t& c_config)
{
    TimepulsePinConfig cpp_config;
    cpp_config.active = c_config.active;
    cpp_config.fixedPulse.frequency = c_config.fixed_pulse.frequency;
    cpp_config.fixedPulse.pulseWidth = c_config.fixed_pulse.pulse_width;

    if (c_config.has_pulse_when_no_fix)
    {
        TimepulsePinConfig::Pulse pulse;
        pulse.frequency = c_config.pulse_when_no_fix.frequency;
        pulse.pulseWidth = c_config.pulse_when_no_fix.pulse_width;
        cpp_config.pulseWhenNoFix = pulse;
    }

    cpp_config.polarity = convert_timepulse_polarity(c_config.polarity);
    return cpp_config;
}

std::optional<BaseConfig> convert_base_config(
    const jp_gnss_base_config_t& c_base)
{
    BaseConfig base;

    if (c_base.base_mode == JP_GNSS_BASE_MODE_SURVEY_IN)
    {
        base.mode = BaseConfig::SurveyIn {
            .minimumObservationTime_s =
                c_base.survey_in.minimum_observation_time_s,
            .requiredPositionAccuracy_m =
                c_base.survey_in.required_position_accuracy_m
        };
    }
    else if (c_base.base_mode == JP_GNSS_BASE_MODE_FIXED_POSITION)
    {
        BaseConfig::FixedPosition fp;
        fp.positionAccuracy_m =
            c_base.fixed_position.position_accuracy_m;

        if (c_base.fixed_position.position_type ==
            JP_GNSS_FIXED_POSITION_ECEF)
        {
            fp.position = BaseConfig::FixedPosition::Ecef {
                .x_m = c_base.fixed_position.ecef.x_m,
                .y_m = c_base.fixed_position.ecef.y_m,
                .z_m = c_base.fixed_position.ecef.z_m
            };
        }
        else if (c_base.fixed_position.position_type ==
                 JP_GNSS_FIXED_POSITION_LLA)
        {
            fp.position = BaseConfig::FixedPosition::Lla {
                .latitude_deg =
                    c_base.fixed_position.lla.latitude_deg,
                .longitude_deg =
                    c_base.fixed_position.lla.longitude_deg,
                .height_m =
                    c_base.fixed_position.lla.height_m
            };
        }
        else
        {
            fprintf(stderr,
                "[GnssHat C API] Invalid fixed position type: %d, "
                "expected ECEF (%d) or LLA (%d)\r\n",
                c_base.fixed_position.position_type,
                JP_GNSS_FIXED_POSITION_ECEF,
                JP_GNSS_FIXED_POSITION_LLA);
            return std::nullopt;
        }

        base.mode = fp;
    }
    else
    {
        fprintf(stderr,
            "[GnssHat C API] Invalid base mode: %d, "
            "expected SURVEY_IN (%d) or FIXED_POSITION (%d)\r\n",
            c_base.base_mode,
            JP_GNSS_BASE_MODE_SURVEY_IN,
            JP_GNSS_BASE_MODE_FIXED_POSITION);
        return std::nullopt;
    }

    return base;
}

std::optional<RtkConfig> convert_rtk_config(
    const jp_gnss_rtk_config_t& c_rtk)
{
    RtkConfig rtk;
    rtk.mode = static_cast<ERtkMode>(c_rtk.mode);

    if (c_rtk.has_base_config)
    {
        auto base = convert_base_config(c_rtk.base);
        if (!base)
            return std::nullopt;
        rtk.base = *base;
    }

    return rtk;
}

std::optional<GnssConfig> convert_gnss_config(
    const jp_gnss_gnss_config_t& c_config)
{
    GnssConfig cpp_config;
    cpp_config.measurementRate_Hz = c_config.measurement_rate_hz;
    cpp_config.dynamicModel = convert_dynamic_model(c_config.dynamic_model);
    cpp_config.timepulsePinConfig =
        convert_timepulse_config(c_config.timepulse_pin_config);

    if (c_config.has_geofencing && c_config.geofencing.geofence_count > 0)
    {
        GnssConfig::Geofencing geofencing;
        geofencing.confidenceLevel = c_config.geofencing.confidence_level;

        if (c_config.geofencing.has_pin_polarity)
        {
            geofencing.pioPinPolarity =
                convert_pio_pin_polarity(c_config.geofencing.pin_polarity);
        }

        for (
            uint8_t i = 0;
            i < c_config.geofencing.geofence_count && i < UBLOX_MAX_GEOFENCES;
            i++)
        {
            Geofence geofence;
            geofence.lat = c_config.geofencing.geofences[i].lat;
            geofence.lon = c_config.geofencing.geofences[i].lon;
            geofence.radius = c_config.geofencing.geofences[i].radius;
            geofencing.geofences.push_back(geofence);
        }

        cpp_config.geofencing = geofencing;
    }

    if (c_config.has_rtk)
    {
        auto rtk = convert_rtk_config(c_config.rtk);
        if (!rtk)
            return std::nullopt;
        cpp_config.rtk = *rtk;
    }

    if (c_config.has_timing)
    {
        TimingConfig timing;
        timing.enableTimeMark = c_config.timing.enable_time_mark;

        if (c_config.timing.has_time_base)
        {
            auto timeBase = convert_base_config(c_config.timing.time_base);
            if (!timeBase)
                return std::nullopt;
            timing.timeBase = *timeBase;
        }

        cpp_config.timing = timing;
    }

    if (c_config.has_navigation_filters)
    {
        const auto& src = c_config.navigation_filters;
        GnssConfig::NavigationFilters f;
        if (src.has_min_svs)        f.minSvs        = src.min_svs;
        if (src.has_max_svs)        f.maxSvs        = src.max_svs;
        if (src.has_min_cno_dbhz)   f.minCno_dBHz   = src.min_cno_dbhz;
        if (src.has_min_elev_deg)   f.minElev_deg   = src.min_elev_deg;
        if (src.has_n_cno_thrs)     f.nCnoThrs      = src.n_cno_thrs;
        if (src.has_cno_thrs_dbhz)  f.cnoThrs_dBHz  = src.cno_thrs_dbhz;
        if (src.has_fix_mode)
            f.fixMode =
                static_cast<GnssConfig::NavigationFilters::FixMode>(
                    src.fix_mode);
        if (src.has_pdop_mask_x10)  f.pdopMask_x10  = src.pdop_mask_x10;
        if (src.has_tdop_mask_x10)  f.tdopMask_x10  = src.tdop_mask_x10;
        if (src.has_p_acc_mask_m)   f.pAccMask_m    = src.p_acc_mask_m;
        if (src.has_t_acc_mask_m)   f.tAccMask_m    = src.t_acc_mask_m;
        cpp_config.navigationFilters = f;
    }

    cpp_config.saveToFlash = c_config.save_to_flash;
 
    return cpp_config;
}

jp_gnss_navigation_t convert_navigation(const Navigation& cpp_nav)
{
    jp_gnss_navigation_t c_nav;

    c_nav.dop.geometric = cpp_nav.dop.geometric;
    c_nav.dop.position = cpp_nav.dop.position;
    c_nav.dop.time = cpp_nav.dop.time;
    c_nav.dop.vertical = cpp_nav.dop.vertical;
    c_nav.dop.horizontal = cpp_nav.dop.horizontal;
    c_nav.dop.northing = cpp_nav.dop.northing;
    c_nav.dop.easting = cpp_nav.dop.easting;
    
    c_nav.pvt.fix_quality = convert_fix_quality(cpp_nav.pvt.fixQuality);
    c_nav.pvt.fix_status = convert_fix_status(cpp_nav.pvt.fixStatus);
    c_nav.pvt.fix_type = convert_fix_type(cpp_nav.pvt.fixType);
    
    c_nav.pvt.utc.hh = cpp_nav.pvt.utc.hh;
    c_nav.pvt.utc.mm = cpp_nav.pvt.utc.mm;
    c_nav.pvt.utc.ss = cpp_nav.pvt.utc.ss;
    c_nav.pvt.utc.valid = cpp_nav.pvt.utc.valid;
    c_nav.pvt.utc.accuracy = cpp_nav.pvt.utc.accuracy;
    
    c_nav.pvt.date.day = cpp_nav.pvt.date.day;
    c_nav.pvt.date.month = cpp_nav.pvt.date.month;
    c_nav.pvt.date.year = cpp_nav.pvt.date.year;
    c_nav.pvt.date.valid = cpp_nav.pvt.date.valid;
    
    c_nav.pvt.altitude = cpp_nav.pvt.altitude;
    c_nav.pvt.altitude_msl = cpp_nav.pvt.altitudeMSL;
    c_nav.pvt.latitude = cpp_nav.pvt.latitude;
    c_nav.pvt.longitude = cpp_nav.pvt.longitude;
    c_nav.pvt.speed_over_ground = cpp_nav.pvt.speedOverGround;
    c_nav.pvt.speed_accuracy = cpp_nav.pvt.speedAccuracy;
    c_nav.pvt.heading = cpp_nav.pvt.heading;
    c_nav.pvt.heading_accuracy = cpp_nav.pvt.headingAccuracy;
    c_nav.pvt.visible_satellites = cpp_nav.pvt.visibleSatellites;
    c_nav.pvt.horizontal_accuracy = cpp_nav.pvt.horizontalAccuracy;
    c_nav.pvt.vertical_accuracy = cpp_nav.pvt.verticalAccuracy;
    
    c_nav.geofencing.cfg.pio_pin_number = cpp_nav.geofencing.cfg.pioPinNumber;
    c_nav.geofencing.cfg.pin_polarity = convert_pio_pin_polarity(
        cpp_nav.geofencing.cfg.pinPolarity);
    c_nav.geofencing.cfg.pio_enabled = cpp_nav.geofencing.cfg.pioEnabled;
    c_nav.geofencing.cfg.confidence_level =
        cpp_nav.geofencing.cfg.confidenceLevel;
    
    c_nav.geofencing.cfg.geofence_count = std::min(
        static_cast<size_t>(UBLOX_MAX_GEOFENCES),
        cpp_nav.geofencing.cfg.geofences.size()
    );

    for (uint8_t i = 0; i < c_nav.geofencing.cfg.geofence_count; ++i)
    {
        const auto& cpp_geofence = cpp_nav.geofencing.cfg.geofences[i];
        c_nav.geofencing.cfg.geofences[i].lat = cpp_geofence.lat;
        c_nav.geofencing.cfg.geofences[i].lon = cpp_geofence.lon;
        c_nav.geofencing.cfg.geofences[i].radius = cpp_geofence.radius;
    }

    c_nav.geofencing.nav.iTOW = cpp_nav.geofencing.nav.iTOW;
    c_nav.geofencing.nav.geofencing_status = convert_geofencing_status(
        cpp_nav.geofencing.nav.geofencingStatus);
    c_nav.geofencing.nav.number_of_geofences =
        cpp_nav.geofencing.nav.numberOfGeofences;
    c_nav.geofencing.nav.combined_state = convert_geofence_status(
        cpp_nav.geofencing.nav.combinedState);

    const auto geofences_size = cpp_nav.geofencing.nav.geofencesStatus.size();
    for (size_t i = 0; i < UBLOX_MAX_GEOFENCES && i < geofences_size; ++i)
    {
        c_nav.geofencing.nav.geofences_status[i] = convert_geofence_status(
            cpp_nav.geofencing.nav.geofencesStatus[i]);
    }

    c_nav.num_rf_blocks = static_cast<uint8_t>(
        std::min(cpp_nav.rfBlocks.size(),
            static_cast<size_t>(UBLOX_MAX_RF_BLOCKS)));

    for (
        size_t i = 0;
        i < cpp_nav.rfBlocks.size() && i < UBLOX_MAX_RF_BLOCKS;
        i++)
    {
        c_nav.rf_blocks[i].id = cpp_nav.rfBlocks[i].id;
        c_nav.rf_blocks[i].jamming_state = convert_jamming_state(
            cpp_nav.rfBlocks[i].jammingState);
        c_nav.rf_blocks[i].antenna_status = convert_antenna_status(
            cpp_nav.rfBlocks[i].antennaStatus);
        c_nav.rf_blocks[i].antenna_power = convert_antenna_power(
            cpp_nav.rfBlocks[i].antennaPower);
        c_nav.rf_blocks[i].post_status = cpp_nav.rfBlocks[i].postStatus;
        c_nav.rf_blocks[i].noise_per_ms = cpp_nav.rfBlocks[i].noisePerMS;
        c_nav.rf_blocks[i].agc_monitor = cpp_nav.rfBlocks[i].agcMonitor;
        c_nav.rf_blocks[i].cw_interference_suppression_level =
            cpp_nav.rfBlocks[i].cwInterferenceSuppressionLevel;
        c_nav.rf_blocks[i].ofs_i = cpp_nav.rfBlocks[i].ofsI;
        c_nav.rf_blocks[i].mag_i = cpp_nav.rfBlocks[i].magI;
        c_nav.rf_blocks[i].ofs_q = cpp_nav.rfBlocks[i].ofsQ;
        c_nav.rf_blocks[i].mag_q = cpp_nav.rfBlocks[i].magQ;
        c_nav.rf_blocks[i].gnss_band = convert_rf_band(cpp_nav.rfBlocks[i].gnssBand);
    }

    c_nav.num_rf_blocks_spectrum = static_cast<uint8_t>(
        std::min(cpp_nav.rfBlocksSpectrumData.size(),
            static_cast<size_t>(UBLOX_MAX_RF_BLOCKS)));

    for (
        size_t i = 0;
        i < cpp_nav.rfBlocksSpectrumData.size() && i < UBLOX_MAX_RF_BLOCKS;
        i++)
    {
        c_nav.rf_blocks_spectrum[i].id = cpp_nav.rfBlocksSpectrumData[i].id;
        const auto& src = cpp_nav.rfBlocksSpectrumData[i].data;
        size_t copy_len = std::min(src.size(), static_cast<size_t>(UBLOX_SPECTRUM_BINS));
        std::memcpy(c_nav.rf_blocks_spectrum[i].data, src.data(), copy_len);
        c_nav.rf_blocks_spectrum[i].span = cpp_nav.rfBlocksSpectrumData[i].span;
        c_nav.rf_blocks_spectrum[i].resolution = cpp_nav.rfBlocksSpectrumData[i].resolution;
        c_nav.rf_blocks_spectrum[i].center_freq = cpp_nav.rfBlocksSpectrumData[i].centerFreq;
        c_nav.rf_blocks_spectrum[i].gain = cpp_nav.rfBlocksSpectrumData[i].gain;
    }

    c_nav.num_satellites = static_cast<uint8_t>(
        std::min(cpp_nav.satellites.size(),
            static_cast<size_t>(UBLOX_MAX_SATELLITES)));

    for (
        size_t i = 0;
        i < cpp_nav.satellites.size() && i < UBLOX_MAX_SATELLITES;
        i++)
    {
        c_nav.satellites[i].gnss_id = convert_gnss_id(
            cpp_nav.satellites[i].gnssId);
        c_nav.satellites[i].sv_id = cpp_nav.satellites[i].svId;
        c_nav.satellites[i].cno = cpp_nav.satellites[i].cno;
        c_nav.satellites[i].elevation = cpp_nav.satellites[i].elevation;
        c_nav.satellites[i].azimuth = cpp_nav.satellites[i].azimuth;
        c_nav.satellites[i].quality = convert_sv_quality(
            cpp_nav.satellites[i].quality);
        c_nav.satellites[i].used_in_fix = cpp_nav.satellites[i].usedInFix;
        c_nav.satellites[i].healthy = cpp_nav.satellites[i].healthy;
        c_nav.satellites[i].diff_corr = cpp_nav.satellites[i].diffCorr;
        c_nav.satellites[i].eph_avail = cpp_nav.satellites[i].ephAvail;
        c_nav.satellites[i].alm_avail = cpp_nav.satellites[i].almAvail;
    }

    return c_nav;
}

jp_gnss_time_mark_t convert_time_mark(const TimeMark& cpp_tm)
{
    jp_gnss_time_mark_t c_tm;
    c_tm.channel = cpp_tm.channel;
    c_tm.mode = static_cast<jp_gnss_time_mark_mode_t>(cpp_tm.mode);
    c_tm.run = static_cast<jp_gnss_time_mark_run_t>(cpp_tm.run);
    c_tm.new_falling_edge = cpp_tm.newFallingEdge;
    c_tm.time_base = static_cast<jp_gnss_time_mark_time_base_t>(cpp_tm.timeBase);
    c_tm.utc_available = cpp_tm.utcAvailable;
    c_tm.time_valid = cpp_tm.timeValid;
    c_tm.new_rising_edge = cpp_tm.newRisingEdge;
    c_tm.count = cpp_tm.count;
    c_tm.week_number_rising = cpp_tm.weekNumberRising;
    c_tm.week_number_falling = cpp_tm.weekNumberFalling;
    c_tm.tow_rising_ms = cpp_tm.towRising_ms;
    c_tm.tow_sub_rising_ns = cpp_tm.towSubRising_ns;
    c_tm.tow_falling_ms = cpp_tm.towFalling_ms;
    c_tm.tow_sub_falling_ns = cpp_tm.towSubFalling_ns;
    c_tm.accuracy_estimate_ns = cpp_tm.accuracyEstimate_ns;
    return c_tm;
}

}  // anonymous namespace

struct jp_gnss_hat
{
    IGnssHat* instance;
};

extern "C"
{

jp_gnss_hat_t* jp_gnss_hat_create()
{
    try
    {
        auto* wrapper = new jp_gnss_hat;
        wrapper->instance = IGnssHat::create();
        return wrapper;
    }
    catch (...)
    {
        return nullptr;
    }
}

void jp_gnss_hat_destroy(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (hat->instance)
    {
        delete hat->instance;
        hat->instance = nullptr;
    }
    delete hat;
}

bool jp_gnss_hat_start(jp_gnss_hat_t* hat,
    const jp_gnss_gnss_config_t* config)
{
    if (!hat || !config)
        return false;

    if (!hat->instance)
        return false;

    auto cpp_config = convert_gnss_config(*config);
    if (!cpp_config)
        return false;

    return hat->instance->start(*cpp_config);
}

bool jp_gnss_hat_wait_and_get_fresh_navigation(jp_gnss_hat_t* hat,
    jp_gnss_navigation_t* navigation)
{
    if (!hat || !navigation)
        return false;

    if (!hat->instance)
        return false;

    *navigation =
        convert_navigation(hat->instance->waitAndGetFreshNavigation());
    return true;
}

bool jp_gnss_hat_get_navigation(jp_gnss_hat_t* hat,
    jp_gnss_navigation_t* navigation)
{
    if (!hat || !navigation)
        return false;

    if (!hat->instance)
        return false;

    *navigation = convert_navigation(hat->instance->navigation());
    return true;
}

bool jp_gnss_hat_get_system_health(jp_gnss_hat_t* hat,
    jp_gnss_system_health_t* system_health)
{
    if (!hat || !system_health || !hat->instance)
        return false;

    const SystemHealth s = hat->instance->systemHealth();
    system_health->valid          = s.valid;
    system_health->msg_version    = s.msgVersion;
    system_health->boot_type      =
        static_cast<jp_gnss_boot_type_t>(s.bootType);
    system_health->cpu_load       = s.cpuLoad;
    system_health->cpu_load_max   = s.cpuLoadMax;
    system_health->mem_usage      = s.memUsage;
    system_health->mem_usage_max  = s.memUsageMax;
    system_health->io_usage       = s.ioUsage;
    system_health->io_usage_max   = s.ioUsageMax;
    system_health->run_time_s     = s.runTime;
    system_health->notice_count   = s.noticeCount;
    system_health->warn_count     = s.warnCount;
    system_health->error_count    = s.errorCount;
    system_health->temperature_c  = s.temperatureC;
    return true;
}

bool jp_gnss_hat_get_mon_ver(jp_gnss_hat_t* hat,
    jp_gnss_mon_ver_t* mon_ver)
{
    if (!hat || !mon_ver || !hat->instance)
        return false;

    const std::string sw = hat->instance->swVersion();
    const std::string hw = hat->instance->hwVersion();
    const auto exts = hat->instance->monVerExtensions();

    std::memset(mon_ver, 0, sizeof(*mon_ver));
    mon_ver->valid = !sw.empty() || !hw.empty() || !exts.empty();

    auto copy_truncated = [](char* dst, size_t dstSize, const std::string& src)
    {
        const size_t n = std::min(src.size(), dstSize - 1);
        std::memcpy(dst, src.data(), n);
        dst[n] = '\0';
    };

    copy_truncated(mon_ver->sw_version, JP_GNSS_MON_VER_STR_MAX, sw);
    copy_truncated(mon_ver->hw_version, JP_GNSS_MON_VER_STR_MAX, hw);

    const size_t nExt = std::min<size_t>(
        exts.size(), JP_GNSS_MON_VER_MAX_EXTENSIONS);
    mon_ver->num_extensions = static_cast<uint8_t>(nExt);
    for (size_t i = 0; i < nExt; ++i)
    {
        copy_truncated(
            mon_ver->extensions[i], JP_GNSS_MON_VER_STR_MAX, exts[i]);
    }
    return true;
}

bool jp_gnss_hat_enable_timepulse(jp_gnss_hat_t* hat)
{
    if (!hat)
        return false;

    if (!hat->instance)
        return false;

    return hat->instance->enableTimepulse();
}

void jp_gnss_hat_disable_timepulse(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (!hat->instance)
        return;

    hat->instance->disableTimepulse();
}

bool jp_gnss_hat_start_forward_for_gpsd(jp_gnss_hat_t* hat)
{
    if (!hat)
        return false;

    if (!hat->instance)
        return false;

    return hat->instance->startForwardForGpsd();
}

void jp_gnss_hat_stop_forward_for_gpsd(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (!hat->instance)
        return;

    hat->instance->stopForwardForGpsd();
}

void jp_gnss_hat_join_forward_for_gpsd(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (!hat->instance)
        return;

    hat->instance->joinForwardForGpsd();
}

const char* jp_gnss_hat_get_gpsd_device_path(jp_gnss_hat_t* hat)
{
    if (!hat)
        return nullptr;

    if (!hat->instance)
        return nullptr;

    thread_local std::string path;
    path = hat->instance->getGpsdDevicePath();
    return path.c_str();
}

void jp_gnss_hat_hard_reset_cold_start(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (hat->instance)
    {
        hat->instance->hardResetUbloxSom_ColdStart();
    }
}

void jp_gnss_hat_soft_reset_hot_start(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (hat->instance)
    {
        hat->instance->softResetUbloxSom_HotStart();
    }
}

bool jp_gnss_gnss_config_add_geofence(jp_gnss_gnss_config_t* config,
    jp_gnss_geofence_t geofence)
{
    if (!config || config->geofencing.geofence_count >= UBLOX_MAX_GEOFENCES)
        return false;

    const uint8_t idx = config->geofencing.geofence_count;
    config->geofencing.geofences[idx].lat = geofence.lat;
    config->geofencing.geofences[idx].lon = geofence.lon;
    config->geofencing.geofences[idx].radius = geofence.radius;
    config->geofencing.geofence_count++;
    config->has_geofencing = true;

    return true;
}

const char* jp_gnss_jamming_state_to_string(jp_gnss_jamming_state_t state)
{
    thread_local std::string result;
    result = Utils::jammingState2string(convert_jamming_state(state));
    return result.c_str();
}

const char* jp_gnss_fix_quality_to_string(jp_gnss_fix_quality_t quality)
{
    thread_local std::string result;
    result = Utils::eFixQuality2string(convert_fix_quality(quality));
    return result.c_str();
}

const char* jp_gnss_fix_status_to_string(jp_gnss_fix_status_t status)
{
    thread_local std::string result;
    result = Utils::eFixStatus2string(convert_fix_status(status));
    return result.c_str();
}

const char* jp_gnss_fix_type_to_string(jp_gnss_fix_type_t type)
{
    thread_local std::string result;
    result = Utils::eFixType2string(convert_fix_type(type));
    return result.c_str();
}

const char* jp_gnss_antenna_status_to_string(jp_gnss_antenna_status_t status)
{
    thread_local std::string result;
    result = Utils::antennaStatus2string(convert_antenna_status(status));
    return result.c_str();
}

const char* jp_gnss_antenna_power_to_string(jp_gnss_antenna_power_t power)
{
    thread_local std::string result;
    result = Utils::antennaPower2string(convert_antenna_power(power));
    return result.c_str();
}

const char* jp_gnss_rf_band_to_string(jp_gnss_rf_band_t band)
{
    thread_local std::string result;
    result = Utils::eBand2string(convert_rf_band(band));
    return result.c_str();
}

const char* jp_gnss_geofencing_status_to_string(
    jp_gnss_geofencing_status_t status)
{
    thread_local std::string result;
    result = Utils::geofencingStatus2string(convert_geofencing_status(status));
    return result.c_str();
}

const char* jp_gnss_geofence_status_to_string(jp_gnss_geofence_status_t status)
{
    thread_local std::string result;
    result = Utils::geofenceStatus2string(convert_geofence_status(status));
    return result.c_str();
}

const char* jp_gnss_gnss_id_to_string(jp_gnss_gnss_id_t id)
{
    thread_local std::string result;
    result = Utils::gnssId2string(static_cast<EGnssId>(id));
    return result.c_str();
}

const char* jp_gnss_sv_quality_to_string(jp_gnss_sv_quality_t quality)
{
    thread_local std::string result;
    result = Utils::svQuality2string(static_cast<ESvQuality>(quality));
    return result.c_str();
}

void jp_gnss_gnss_config_init(jp_gnss_gnss_config_t* config)
{
    if (!config)
        return;

    std::memset(config, 0, sizeof(*config));

    config->measurement_rate_hz = 1;
    config->dynamic_model = JP_GNSS_DYNAMIC_MODEL_PORTABLE;
    config->timepulse_pin_config.active = true;
    config->timepulse_pin_config.fixed_pulse.frequency = 1;
    config->timepulse_pin_config.fixed_pulse.pulse_width = 0.1f;
    config->timepulse_pin_config.has_pulse_when_no_fix = false;
    config->timepulse_pin_config.polarity =
        JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;
    config->has_geofencing = false;
    config->has_rtk = false;
    std::memset(&config->rtk, 0, sizeof(config->rtk));
    config->has_timing = false;
    std::memset(&config->timing, 0, sizeof(config->timing));
    config->has_navigation_filters = false;
    std::memset(
        &config->navigation_filters, 0, sizeof(config->navigation_filters));
    config->save_to_flash = false;
}

void jp_gnss_hat_timepulse(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (!hat->instance)
        return;

    hat->instance->timepulse();
}

const char* jp_gnss_hat_name(jp_gnss_hat_t* hat)
{
    if (!hat || !hat->instance)
        return nullptr;

    return hat->instance->name().data();
}

jp_gnss_rtk_corrections_t* jp_gnss_rtk_get_full_corrections(
    jp_gnss_hat_t* hat)
{
    if (!hat || !hat->instance)
        return nullptr;

    IRtk* rtk = hat->instance->rtk();
    if (!rtk)
        return nullptr;

    IBase* base = rtk->base();
    if (!base)
        return nullptr;

    auto cpp_corrections = base->getFullCorrections();
    if (cpp_corrections.empty())
        return nullptr;

    auto* result = new (std::nothrow) jp_gnss_rtk_corrections_t;
    if (!result)
        return nullptr;

    result->count = static_cast<uint32_t>(cpp_corrections.size());
    result->frames =
        new (std::nothrow) jp_gnss_rtcm3_frame_t[result->count];
    if (!result->frames)
    {
        delete result;
        return nullptr;
    }

    for (uint32_t i = 0; i < result->count; ++i)
    {
        result->frames[i].size =
            static_cast<uint32_t>(cpp_corrections[i].size());
        result->frames[i].data =
            new (std::nothrow) uint8_t[result->frames[i].size];
        if (!result->frames[i].data)
        {
            for (uint32_t j = 0; j < i; ++j)
                delete[] result->frames[j].data;
            delete[] result->frames;
            delete result;
            return nullptr;
        }
        std::memcpy(result->frames[i].data, cpp_corrections[i].data(),
            result->frames[i].size);
    }

    return result;
}

jp_gnss_rtk_corrections_t* jp_gnss_rtk_get_tiny_corrections(
    jp_gnss_hat_t* hat)
{
    if (!hat || !hat->instance)
        return nullptr;

    IRtk* rtk = hat->instance->rtk();
    if (!rtk)
        return nullptr;

    IBase* base = rtk->base();
    if (!base)
        return nullptr;

    auto cpp_corrections = base->getTinyCorrections();
    if (cpp_corrections.empty())
        return nullptr;

    auto* result = new (std::nothrow) jp_gnss_rtk_corrections_t;
    if (!result)
        return nullptr;

    result->count = static_cast<uint32_t>(cpp_corrections.size());
    result->frames =
        new (std::nothrow) jp_gnss_rtcm3_frame_t[result->count];
    if (!result->frames)
    {
        delete result;
        return nullptr;
    }

    for (uint32_t i = 0; i < result->count; ++i)
    {
        result->frames[i].size =
            static_cast<uint32_t>(cpp_corrections[i].size());
        result->frames[i].data =
            new (std::nothrow) uint8_t[result->frames[i].size];
        if (!result->frames[i].data)
        {
            for (uint32_t j = 0; j < i; ++j)
                delete[] result->frames[j].data;
            delete[] result->frames;
            delete result;
            return nullptr;
        }
        std::memcpy(result->frames[i].data, cpp_corrections[i].data(),
            result->frames[i].size);
    }

    return result;
}

jp_gnss_rtcm3_frame_t* jp_gnss_rtk_get_rtcm3_frame(jp_gnss_hat_t* hat,
    uint16_t id)
{
    if (!hat || !hat->instance)
        return nullptr;

    IRtk* rtk = hat->instance->rtk();
    if (!rtk)
        return nullptr;

    IBase* base = rtk->base();
    if (!base)
        return nullptr;

    auto cpp_frame = base->getRtcm3Frame(id);
    if (cpp_frame.empty())
        return nullptr;

    auto* result = new (std::nothrow) jp_gnss_rtcm3_frame_t;
    if (!result)
        return nullptr;

    result->size = static_cast<uint32_t>(cpp_frame.size());
    result->data = new (std::nothrow) uint8_t[result->size];
    if (!result->data)
    {
        delete result;
        return nullptr;
    }

    std::memcpy(result->data, cpp_frame.data(), result->size);
    return result;
}

bool jp_gnss_rtk_apply_corrections(jp_gnss_hat_t* hat,
    const jp_gnss_rtcm3_frame_t* frames, uint32_t count)
{
    if (!hat || !hat->instance || !frames || count == 0)
        return false;

    IRtk* rtk = hat->instance->rtk();
    if (!rtk)
        return false;

    IRover* rover = rtk->rover();
    if (!rover)
        return false;

    std::vector<std::vector<uint8_t>> cpp_corrections;
    cpp_corrections.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        cpp_corrections.emplace_back(
            frames[i].data, frames[i].data + frames[i].size);
    }

    rover->applyCorrections(cpp_corrections);
    return true;
}

void jp_gnss_rtk_corrections_free(jp_gnss_rtk_corrections_t* corrections)
{
    if (!corrections)
        return;

    if (corrections->frames)
    {
        for (uint32_t i = 0; i < corrections->count; ++i)
            delete[] corrections->frames[i].data;
        delete[] corrections->frames;
    }
    delete corrections;
}

void jp_gnss_rtcm3_frame_free(jp_gnss_rtcm3_frame_t* frame)
{
    if (!frame)
        return;

    delete[] frame->data;
    delete frame;
}

const char* jp_gnss_utc_time_iso8601(
    const jp_gnss_position_velocity_time_t* pvt)
{
    if (!pvt)
        return "";

    PositionVelocityTime cpp_pvt{};
    cpp_pvt.date.year = pvt->date.year;
    cpp_pvt.date.month = pvt->date.month;
    cpp_pvt.date.day = pvt->date.day;
    cpp_pvt.utc.hh = pvt->utc.hh;
    cpp_pvt.utc.mm = pvt->utc.mm;
    cpp_pvt.utc.ss = pvt->utc.ss;

    thread_local std::string result;
    result = Utils::utcTimeFromGnss_ISO8601(cpp_pvt);
    return result.c_str();
}

bool jp_gnss_hat_get_time_mark(jp_gnss_hat_t* hat,
    jp_gnss_time_mark_t* time_mark)
{
    if (!hat || !time_mark || !hat->instance)
        return false;

    auto opt = hat->instance->timeMark();
    if (!opt.has_value())
        return false;

    *time_mark = convert_time_mark(opt.value());
    return true;
}

bool jp_gnss_hat_wait_and_get_fresh_time_mark(jp_gnss_hat_t* hat,
    jp_gnss_time_mark_t* time_mark)
{
    if (!hat || !time_mark || !hat->instance)
        return false;

    *time_mark = convert_time_mark(
        hat->instance->waitAndGetFreshTimeMark());
    return true;
}

bool jp_gnss_hat_enable_time_mark_trigger(jp_gnss_hat_t* hat)
{
    if (!hat || !hat->instance)
        return false;

    return hat->instance->enableTimeMarkTrigger();
}

void jp_gnss_hat_disable_time_mark_trigger(jp_gnss_hat_t* hat)
{
    if (!hat || !hat->instance)
        return;

    hat->instance->disableTimeMarkTrigger();
}

void jp_gnss_hat_trigger_time_mark(jp_gnss_hat_t* hat,
    jp_gnss_time_mark_trigger_edge_t edge)
{
    if (!hat || !hat->instance)
        return;

    hat->instance->triggerTimeMark(
        static_cast<ETimeMarkTriggerEdge>(edge));
}

const char* jp_gnss_time_mark_mode_to_string(
    jp_gnss_time_mark_mode_t mode)
{
    thread_local std::string result;
    result = Utils::timeMarkMode2string(static_cast<ETimeMarkMode>(mode));
    return result.c_str();
}

const char* jp_gnss_time_mark_run_to_string(
    jp_gnss_time_mark_run_t run)
{
    thread_local std::string result;
    result = Utils::timeMarkRun2string(static_cast<ETimeMarkRun>(run));
    return result.c_str();
}

const char* jp_gnss_time_mark_time_base_to_string(
    jp_gnss_time_mark_time_base_t time_base)
{
    thread_local std::string result;
    result = Utils::timeMarkTimeBase2string(
        static_cast<ETimeMarkTimeBase>(time_base));
    return result.c_str();
}

/* ── NTRIP Caster ───────────────────────────────────────────────────── */

struct jp_gnss_ntrip_caster
{
    NtripCaster* instance;
};

jp_gnss_ntrip_caster_t* jp_gnss_ntrip_caster_create(
    const char* host, uint16_t port,
    const char* mountpoint, uint32_t max_clients)
{
    if (!host || !mountpoint)
        return nullptr;

    try
    {
        auto* wrapper = new jp_gnss_ntrip_caster;
        wrapper->instance = new NtripCaster(
            host, port, mountpoint, static_cast<size_t>(max_clients));
        return wrapper;
    }
    catch (...)
    {
        return nullptr;
    }
}

void jp_gnss_ntrip_caster_destroy(jp_gnss_ntrip_caster_t* caster)
{
    if (!caster)
        return;

    delete caster->instance;
    delete caster;
}

bool jp_gnss_ntrip_caster_start(jp_gnss_ntrip_caster_t* caster)
{
    if (!caster || !caster->instance)
        return false;

    return caster->instance->start();
}

void jp_gnss_ntrip_caster_stop(jp_gnss_ntrip_caster_t* caster)
{
    if (!caster || !caster->instance)
        return;

    caster->instance->stop();
}

void jp_gnss_ntrip_caster_feed(jp_gnss_ntrip_caster_t* caster,
    const jp_gnss_rtcm3_frame_t* frames, uint32_t count)
{
    if (!caster || !caster->instance || !frames || count == 0)
        return;

    std::vector<std::vector<uint8_t>> cpp_frames;
    cpp_frames.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        cpp_frames.emplace_back(
            frames[i].data, frames[i].data + frames[i].size);
    }

    caster->instance->feed(cpp_frames);
}

uint32_t jp_gnss_ntrip_caster_client_count(
    const jp_gnss_ntrip_caster_t* caster)
{
    if (!caster || !caster->instance)
        return 0;

    return static_cast<uint32_t>(caster->instance->clientCount());
}

void jp_gnss_ntrip_caster_update_position(
    jp_gnss_ntrip_caster_t* caster, double lat, double lon)
{
    if (!caster || !caster->instance)
        return;

    caster->instance->updatePosition(lat, lon);
}

void jp_gnss_ntrip_caster_set_credentials(
    jp_gnss_ntrip_caster_t* caster,
    const char* username, const char* password)
{
    if (!caster || !caster->instance)
        return;

    caster->instance->setCredentials(
        username ? username : "",
        password ? password : "");
}

void jp_gnss_ntrip_caster_set_log_callback(
    jp_gnss_ntrip_caster_t* caster,
    jp_ntrip_log_callback_t callback, void* user_data)
{
    if (!caster || !caster->instance)
        return;

    if (callback)
    {
        caster->instance->setLogCallback(
            [callback, user_data](ENtripLogLevel level,
                                  const std::string& msg)
            {
                callback(static_cast<jp_ntrip_log_level_t>(level),
                         msg.c_str(), user_data);
            });
    }
    else
    {
        caster->instance->setLogCallback(nullptr);
    }
}

void jp_gnss_ntrip_caster_set_log_level(
    jp_gnss_ntrip_caster_t* caster, jp_ntrip_log_level_t level)
{
    if (!caster || !caster->instance)
        return;

    caster->instance->setLogLevel(static_cast<ENtripLogLevel>(level));
}

/* ── NTRIP Client ───────────────────────────────────────────────────── */

struct jp_gnss_ntrip_client
{
    NtripClient* instance;
};

jp_gnss_ntrip_client_t* jp_gnss_ntrip_client_create(
    const char* host, uint16_t port,
    const char* mountpoint,
    const char* username, const char* password)
{
    if (!host || !mountpoint)
        return nullptr;

    try
    {
        auto* wrapper = new jp_gnss_ntrip_client;
        wrapper->instance = new NtripClient(
            host, port, mountpoint,
            username ? username : "",
            password ? password : "");
        return wrapper;
    }
    catch (...)
    {
        return nullptr;
    }
}

void jp_gnss_ntrip_client_destroy(jp_gnss_ntrip_client_t* client)
{
    if (!client)
        return;

    delete client->instance;
    delete client;
}

bool jp_gnss_ntrip_client_connect(jp_gnss_ntrip_client_t* client)
{
    if (!client || !client->instance)
        return false;

    return client->instance->connect();
}

void jp_gnss_ntrip_client_disconnect(jp_gnss_ntrip_client_t* client)
{
    if (!client || !client->instance)
        return;

    client->instance->disconnect();
}

bool jp_gnss_ntrip_client_is_connected(
    const jp_gnss_ntrip_client_t* client)
{
    if (!client || !client->instance)
        return false;

    return client->instance->isConnected();
}

uint32_t jp_gnss_ntrip_client_receive(
    jp_gnss_ntrip_client_t* client,
    jp_gnss_rtcm3_frame_t** frames_out)
{
    if (!client || !client->instance || !frames_out)
    {
        if (frames_out) *frames_out = nullptr;
        return 0;
    }

    auto cpp_frames = client->instance->receiveFrames();
    if (cpp_frames.empty())
    {
        *frames_out = nullptr;
        return 0;
    }

    uint32_t count = static_cast<uint32_t>(cpp_frames.size());
    auto* frames = static_cast<jp_gnss_rtcm3_frame_t*>(
        calloc(count, sizeof(jp_gnss_rtcm3_frame_t)));
    if (!frames)
    {
        *frames_out = nullptr;
        return 0;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        frames[i].size = static_cast<uint32_t>(cpp_frames[i].size());
        frames[i].data = static_cast<uint8_t*>(malloc(frames[i].size));
        if (frames[i].data)
            memcpy(frames[i].data, cpp_frames[i].data(), frames[i].size);
    }

    *frames_out = frames;
    return count;
}

void jp_gnss_ntrip_client_free_frames(
    jp_gnss_rtcm3_frame_t* frames, uint32_t count)
{
    if (!frames)
        return;

    for (uint32_t i = 0; i < count; ++i)
        free(frames[i].data);
    free(frames);
}

void jp_gnss_ntrip_client_send_position(
    jp_gnss_ntrip_client_t* client,
    double lat, double lon, double alt)
{
    if (!client || !client->instance)
        return;

    client->instance->sendPosition(lat, lon, alt);
}

void jp_gnss_ntrip_client_set_log_callback(
    jp_gnss_ntrip_client_t* client,
    jp_ntrip_log_callback_t callback, void* user_data)
{
    if (!client || !client->instance)
        return;

    if (callback)
    {
        client->instance->setLogCallback(
            [callback, user_data](ENtripLogLevel level,
                                  const std::string& msg)
            {
                callback(static_cast<jp_ntrip_log_level_t>(level),
                         msg.c_str(), user_data);
            });
    }
    else
    {
        client->instance->setLogCallback(nullptr);
    }
}

void jp_gnss_ntrip_client_set_log_level(
    jp_gnss_ntrip_client_t* client, jp_ntrip_log_level_t level)
{
    if (!client || !client->instance)
        return;

    client->instance->setLogLevel(static_cast<ENtripLogLevel>(level));
}

// ── Stats helpers ──────────────────────────────────────────────────────

static void fillStats(const NtripStats& src, jp_ntrip_stats_t* dst)
{
    dst->bytes_tx = src.bytesTx;
    dst->bytes_rx = src.bytesRx;
    dst->frames_tx = src.framesTx;
    dst->frames_rx = src.framesRx;
    dst->uptime_ms = src.uptimeMs;
    dst->last_frame_age_ms = src.lastFrameAgeMs;
    dst->avg_inter_frame_ms = src.avgInterFrameMs;
    dst->max_inter_frame_ms = src.maxInterFrameMs;

    dst->num_msg_types = 0;
    for (const auto& [id, count] : src.messageTypeCounts)
    {
        if (dst->num_msg_types >= JP_NTRIP_STATS_MAX_MSG_TYPES)
            break;
        dst->msg_type_ids[dst->num_msg_types] = id;
        dst->msg_type_counts[dst->num_msg_types] = count;
        dst->num_msg_types++;
    }
}

void jp_gnss_ntrip_caster_get_stats(
    const jp_gnss_ntrip_caster_t* caster, jp_ntrip_stats_t* stats)
{
    if (!caster || !caster->instance || !stats)
        return;
    memset(stats, 0, sizeof(*stats));
    fillStats(caster->instance->getStats(), stats);
}

void jp_gnss_ntrip_client_get_stats(
    const jp_gnss_ntrip_client_t* client, jp_ntrip_stats_t* stats)
{
    if (!client || !client->instance || !stats)
        return;
    memset(stats, 0, sizeof(*stats));
    fillStats(client->instance->getStats(), stats);
}

void jp_gnss_ntrip_client_set_auto_reconnect(
    jp_gnss_ntrip_client_t* client,
    int enable, uint32_t initial_delay_ms, uint32_t max_delay_ms)
{
    if (!client || !client->instance)
        return;
    client->instance->setAutoReconnect(enable != 0, initial_delay_ms, max_delay_ms);
}

uint32_t jp_gnss_ntrip_client_reconnect_count(
    const jp_gnss_ntrip_client_t* client)
{
    if (!client || !client->instance)
        return 0;
    return client->instance->reconnectCount();
}

/* ── NTRIP Client Auto-GGA ─────────────────────────────────────────── */

void jp_gnss_ntrip_client_update_position(
    jp_gnss_ntrip_client_t* client,
    double lat, double lon, double alt)
{
    if (!client || !client->instance)
        return;
    client->instance->updatePosition(lat, lon, alt);
}

void jp_gnss_ntrip_client_set_auto_gga(
    jp_gnss_ntrip_client_t* client, uint32_t interval_ms)
{
    if (!client || !client->instance)
        return;
    client->instance->setAutoGGA(interval_ms);
}

/* ── NTRIP TLS (client) ────────────────────────────────────────────── */

void jp_gnss_ntrip_client_set_tls(
    jp_gnss_ntrip_client_t* client, int enable, int verify_peer)
{
    if (!client || !client->instance) return;
    client->instance->setUseTls(enable != 0, verify_peer != 0);
}

/* ── NTRIP Server ───────────────────────────────────────────────────── */

struct jp_gnss_ntrip_server
{
    NtripServer* instance;
};

jp_gnss_ntrip_server_t* jp_gnss_ntrip_server_create(
    const char* host, uint16_t port,
    const char* mountpoint, const char* username, const char* password)
{
    if (!host || !mountpoint)
        return nullptr;

    try
    {
        auto* wrapper = new jp_gnss_ntrip_server;
        wrapper->instance = new NtripServer(
            host, port, mountpoint,
            username ? username : "",
            password ? password : "");
        return wrapper;
    }
    catch (...)
    {
        return nullptr;
    }
}

void jp_gnss_ntrip_server_destroy(jp_gnss_ntrip_server_t* server)
{
    if (!server)
        return;

    delete server->instance;
    delete server;
}

bool jp_gnss_ntrip_server_connect(jp_gnss_ntrip_server_t* server)
{
    if (!server || !server->instance)
        return false;

    return server->instance->connect();
}

void jp_gnss_ntrip_server_disconnect(jp_gnss_ntrip_server_t* server)
{
    if (!server || !server->instance)
        return;

    server->instance->disconnect();
}

bool jp_gnss_ntrip_server_is_connected(
    const jp_gnss_ntrip_server_t* server)
{
    if (!server || !server->instance)
        return false;

    return server->instance->isConnected();
}

void jp_gnss_ntrip_server_feed(jp_gnss_ntrip_server_t* server,
    const jp_gnss_rtcm3_frame_t* frames, uint32_t count)
{
    if (!server || !server->instance || !frames || count == 0)
        return;

    std::vector<std::vector<uint8_t>> cpp_frames;
    cpp_frames.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        cpp_frames.emplace_back(
            frames[i].data, frames[i].data + frames[i].size);
    }

    server->instance->feed(cpp_frames);
}

void jp_gnss_ntrip_server_set_auto_reconnect(
    jp_gnss_ntrip_server_t* server,
    int enable, uint32_t initial_delay_ms, uint32_t max_delay_ms)
{
    if (!server || !server->instance)
        return;
    server->instance->setAutoReconnect(enable != 0, initial_delay_ms, max_delay_ms);
}

uint32_t jp_gnss_ntrip_server_reconnect_count(
    const jp_gnss_ntrip_server_t* server)
{
    if (!server || !server->instance)
        return 0;
    return server->instance->reconnectCount();
}

void jp_gnss_ntrip_server_set_log_callback(
    jp_gnss_ntrip_server_t* server,
    jp_ntrip_log_callback_t callback, void* user_data)
{
    if (!server || !server->instance)
        return;

    if (callback)
    {
        server->instance->setLogCallback(
            [callback, user_data](ENtripLogLevel level,
                                  const std::string& msg)
            {
                callback(static_cast<jp_ntrip_log_level_t>(level),
                         msg.c_str(), user_data);
            });
    }
    else
    {
        server->instance->setLogCallback(nullptr);
    }
}

void jp_gnss_ntrip_server_set_log_level(
    jp_gnss_ntrip_server_t* server, jp_ntrip_log_level_t level)
{
    if (!server || !server->instance)
        return;

    server->instance->setLogLevel(static_cast<ENtripLogLevel>(level));
}

void jp_gnss_ntrip_server_get_stats(
    const jp_gnss_ntrip_server_t* server, jp_ntrip_stats_t* stats)
{
    if (!server || !server->instance || !stats)
        return;
    memset(stats, 0, sizeof(*stats));
    fillStats(server->instance->getStats(), stats);
}

/* ── NTRIP TLS (server + availability) ─────────────────────────────── */

void jp_gnss_ntrip_server_set_tls(
    jp_gnss_ntrip_server_t* server, int enable, int verify_peer)
{
    if (!server || !server->instance) return;
    server->instance->setUseTls(enable != 0, verify_peer != 0);
}

bool jp_gnss_ntrip_is_tls_available(void)
{
    return NtripClient::isTlsAvailable();
}

bool jp_gnss_ntrip_caster_set_tls(
    jp_gnss_ntrip_caster_t* caster,
    const char* cert_file, const char* key_file)
{
    if (!caster || !caster->instance || !cert_file || !key_file)
        return false;
    return caster->instance->setTls(cert_file, key_file);
}

/* ── NTRIP Sourcetable Fetch ────────────────────────────────────────── */

static char* strdup_safe(const std::string& s)
{
    char* p = static_cast<char*>(malloc(s.size() + 1));
    if (p)
    {
        memcpy(p, s.c_str(), s.size() + 1);
    }
    return p;
}

uint32_t jp_gnss_ntrip_fetch_sourcetable(
    const char* host, uint16_t port,
    const char* username, const char* password,
    uint32_t timeout_ms,
    jp_ntrip_sourcetable_entry_t** entries_out,
    int use_tls, int tls_verify_peer)
{
    if (!host || !entries_out)
    {
        if (entries_out) *entries_out = nullptr;
        return 0;
    }

    auto cpp_entries = NtripClient::fetchSourcetable(
        host, port,
        username ? username : "",
        password ? password : "",
        timeout_ms,
        use_tls != 0,
        tls_verify_peer != 0);

    if (cpp_entries.empty())
    {
        *entries_out = nullptr;
        return 0;
    }

    uint32_t count = static_cast<uint32_t>(cpp_entries.size());
    auto* entries = static_cast<jp_ntrip_sourcetable_entry_t*>(
        calloc(count, sizeof(jp_ntrip_sourcetable_entry_t)));
    if (!entries)
    {
        *entries_out = nullptr;
        return 0;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        entries[i].mountpoint = strdup_safe(cpp_entries[i].mountpoint);
        entries[i].identifier = strdup_safe(cpp_entries[i].identifier);
        entries[i].format = strdup_safe(cpp_entries[i].format);
        entries[i].format_details = strdup_safe(cpp_entries[i].formatDetails);
        entries[i].carrier = strdup_safe(cpp_entries[i].carrier);
        entries[i].nav_system = strdup_safe(cpp_entries[i].navSystem);
        entries[i].latitude = cpp_entries[i].latitude;
        entries[i].longitude = cpp_entries[i].longitude;
    }

    *entries_out = entries;
    return count;
}

void jp_gnss_ntrip_free_sourcetable(
    jp_ntrip_sourcetable_entry_t* entries, uint32_t count)
{
    if (!entries)
        return;

    for (uint32_t i = 0; i < count; ++i)
    {
        free(entries[i].mountpoint);
        free(entries[i].identifier);
        free(entries[i].format);
        free(entries[i].format_details);
        free(entries[i].carrier);
        free(entries[i].nav_system);
    }
    free(entries);
}

}  // extern "C"
