/*
 * Jimmy Paputto 2025
 */

#include "GnssHat.h"
#include "GnssHat.hpp"

#include <cstring>
#include <new>


using namespace JimmyPaputto;

// Static asserts to ensure C and C++ enum values stay in sync
static_assert(static_cast<int>(EDynamicModel::Portable) == JP_GNSS_DYNAMIC_MODEL_PORTABLE);
static_assert(static_cast<int>(EDynamicModel::Stationary) == JP_GNSS_DYNAMIC_MODEL_STATIONARY);
static_assert(static_cast<int>(EDynamicModel::Pedestrain) == JP_GNSS_DYNAMIC_MODEL_PEDESTRIAN);
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

static_assert(static_cast<int>(EGeofencingStatus::NotAvalaible) == JP_GNSS_GEOFENCING_STATUS_NOT_AVAILABLE);
static_assert(static_cast<int>(EGeofencingStatus::Active) == JP_GNSS_GEOFENCING_STATUS_ACTIVE);

static_assert(static_cast<int>(EBand::L1) == JP_GNSS_RF_BAND_L1);
static_assert(static_cast<int>(EBand::L2orL5) == JP_GNSS_RF_BAND_L2_OR_L5);

static_assert(static_cast<int>(EJammingState::Unknown) == JP_GNSS_JAMMING_STATE_UNKNOWN);
static_assert(static_cast<int>(EJammingState::Ok_NoSignifantJamming) == JP_GNSS_JAMMING_STATE_OK_NO_SIGNIFICANT_JAMMING);
static_assert(static_cast<int>(EJammingState::Warning_InferenceVisibleButFixOk) == JP_GNSS_JAMMING_STATE_WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK);
static_assert(static_cast<int>(EJammingState::Critical_InferenceVisibleAndNoFix) == JP_GNSS_JAMMING_STATE_CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX);

static_assert(static_cast<int>(EAntennaStatus::Init) == JP_GNSS_ANTENNA_STATUS_INIT);
static_assert(static_cast<int>(EAntennaStatus::DontKnow) == JP_GNSS_ANTENNA_STATUS_DONT_KNOW);
static_assert(static_cast<int>(EAntennaStatus::Ok) == JP_GNSS_ANTENNA_STATUS_OK);
static_assert(static_cast<int>(EAntennaStatus::Short) == JP_GNSS_ANTENNA_STATUS_SHORT);
static_assert(static_cast<int>(EAntennaStatus::Open) == JP_GNSS_ANTENNA_STATUS_OPEN);

static_assert(static_cast<int>(EAntennaPower::Off) == JP_GNSS_ANTENNA_POWER_OFF);
static_assert(static_cast<int>(EAntennaPower::On) == JP_GNSS_ANTENNA_POWER_ON);
static_assert(static_cast<int>(EAntennaPower::DontKnow) == JP_GNSS_ANTENNA_POWER_DONT_KNOW);

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

EBand convert_rf_band(jp_gnss_rf_band_t band)
{
    return static_cast<EBand>(band);
}

jp_gnss_rf_band_t convert_rf_band(EBand band)
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

GnssConfig convert_gnss_config(const jp_gnss_gnss_config_t& c_config)
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
    
    c_nav.pvt.quality = convert_fix_quality(cpp_nav.pvt.fixQuality);
    c_nav.pvt.status = convert_fix_status(cpp_nav.pvt.fixStatus);
    c_nav.pvt.type = convert_fix_type(cpp_nav.pvt.fixType);
    
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
        c_nav.rf_blocks[i].id = convert_rf_band(cpp_nav.rfBlocks[i].id);
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
    }

    return c_nav;
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

    return hat->instance->start(convert_gnss_config(*config));
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
}

void jp_gnss_hat_timepulse(jp_gnss_hat_t* hat)
{
    if (!hat)
        return;

    if (!hat->instance)
        return;

    hat->instance->timepulse();
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

    thread_local std::string result = Utils::utcTimeFromGnss_ISO8601(cpp_pvt);
    return result.c_str();
}

}  // extern "C"
