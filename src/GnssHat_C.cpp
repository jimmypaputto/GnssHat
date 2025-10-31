/*
 * Jimmy Paputto 2025
 */

#include "GnssHat.h"
#include "GnssHat.hpp"


using namespace JimmyPaputto;

namespace
{

EDynamicModel convert_dynamic_model(jp_gnss_dynamic_model_t model)
{
    return static_cast<EDynamicModel>(model);
}

jp_gnss_dynamic_model_t convert_dynamic_model(EDynamicModel model)
{
    return static_cast<jp_gnss_dynamic_model_t>(model);
}

ETimepulsePinPolarity convert_timepulse_polarity(
    jp_gnss_timepulse_polarity_t polarity)
{
    return static_cast<ETimepulsePinPolarity>(polarity);
}

jp_gnss_timepulse_polarity_t convert_timepulse_polarity(
    ETimepulsePinPolarity polarity)
{
    return static_cast<jp_gnss_timepulse_polarity_t>(polarity);
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

jp_gnss_timepulse_pin_config_t convert_timepulse_config(
    const TimepulsePinConfig& cpp_config)
{
    jp_gnss_timepulse_pin_config_t c_config;

    c_config.active = cpp_config.active;
    c_config.fixed_pulse.frequency = cpp_config.fixedPulse.frequency;
    c_config.fixed_pulse.pulse_width = cpp_config.fixedPulse.pulseWidth;

    c_config.has_pulse_when_no_fix = cpp_config.pulseWhenNoFix.has_value();
    if (c_config.has_pulse_when_no_fix)
    {
        c_config.pulse_when_no_fix.frequency =
            cpp_config.pulseWhenNoFix->frequency;
        c_config.pulse_when_no_fix.pulse_width =
            cpp_config.pulseWhenNoFix->pulseWidth;
    }

    c_config.polarity = convert_timepulse_polarity(cpp_config.polarity);

    return c_config;
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

    c_nav.num_rf_blocks = static_cast<uint8_t>(cpp_nav.rfBlocks.size());

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

    return hat->instance->getGpsdDevicePath().c_str();
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

} // extern "C"
