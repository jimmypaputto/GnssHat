/*
 * Jimmy Paputto 2025
 * C Language Compatibility Header for GNSS HAT Library
 */

#ifndef GNSS_HAT_H_
#define GNSS_HAT_H_

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

#define UBLOX_MAX_GEOFENCES 4
#define UBLOX_MAX_RF_BLOCKS 2

typedef enum
{
    JP_GNSS_DYNAMIC_MODEL_PORTABLE    = 0,
    JP_GNSS_DYNAMIC_MODEL_STATIONARY  = 2,
    JP_GNSS_DYNAMIC_MODEL_PEDESTRIAN  = 3,
    JP_GNSS_DYNAMIC_MODEL_AUTOMOTIVE  = 4,
    JP_GNSS_DYNAMIC_MODEL_SEA         = 5,
    JP_GNSS_DYNAMIC_MODEL_AIRBORNE_1G = 6,
    JP_GNSS_DYNAMIC_MODEL_AIRBORNE_2G = 7,
    JP_GNSS_DYNAMIC_MODEL_AIRBORNE_4G = 8,
    JP_GNSS_DYNAMIC_MODEL_WRIST       = 9,
    JP_GNSS_DYNAMIC_MODEL_BIKE        = 10,
    JP_GNSS_DYNAMIC_MODEL_MOWER       = 11,
    JP_GNSS_DYNAMIC_MODEL_ESCOOTER    = 12
} jp_gnss_dynamic_model_t;

typedef enum
{
    JP_GNSS_FIX_QUALITY_INVALID        = 0,
    JP_GNSS_FIX_QUALITY_GPS_FIX_2D_3D  = 1,
    JP_GNSS_FIX_QUALITY_DGNSS          = 2,
    JP_GNSS_FIX_QUALITY_PPS_FIX        = 3,
    JP_GNSS_FIX_QUALITY_FIXED_RTK      = 4,
    JP_GNSS_FIX_QUALITY_FLOAT_RTK      = 5,
    JP_GNSS_FIX_QUALITY_DEAD_RECKONING = 6
} jp_gnss_fix_quality_t;

typedef enum
{
    JP_GNSS_FIX_STATUS_VOID   = 0x00,
    JP_GNSS_FIX_STATUS_ACTIVE = 0x01
} jp_gnss_fix_status_t;

typedef enum
{
    JP_GNSS_FIX_TYPE_NO_FIX                   = 0x00,
    JP_GNSS_FIX_TYPE_DEAD_RECKONING_ONLY      = 0x01,
    JP_GNSS_FIX_TYPE_FIX_2D                   = 0x02,
    JP_GNSS_FIX_TYPE_FIX_3D                   = 0x03,
    JP_GNSS_FIX_TYPE_GNSS_WITH_DEAD_RECKONING = 0x04,
    JP_GNSS_FIX_TYPE_TIME_ONLY_FIX            = 0x05
} jp_gnss_fix_type_t;

typedef enum
{
    JP_GNSS_TIMEPULSE_POLARITY_FALLING_EDGE = 0x0,
    JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE  = 0x1
} jp_gnss_timepulse_polarity_t;

typedef enum
{
    JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_INSIDE  = 0x00,
    JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_OUTSIDE = 0x01
} jp_gnss_pio_pin_polarity_t;

typedef enum
{
    JP_GNSS_GEOFENCE_STATUS_UNKNOWN = 0x00,
    JP_GNSS_GEOFENCE_STATUS_INSIDE  = 0x01,
    JP_GNSS_GEOFENCE_STATUS_OUTSIDE = 0x02
} jp_gnss_geofence_status_t;

typedef enum
{
    JP_GNSS_GEOFENCING_STATUS_NOT_AVAILABLE = 0x00,
    JP_GNSS_GEOFENCING_STATUS_ACTIVE        = 0x01
} jp_gnss_geofencing_status_t;

typedef enum
{
    JP_GNSS_RF_BAND_L1       = 0x00,
    JP_GNSS_RF_BAND_L2_OR_L5 = 0x01
} jp_gnss_rf_band_t;

typedef enum
{
    JP_GNSS_JAMMING_STATE_UNKNOWN                                  = 0x00,
    JP_GNSS_JAMMING_STATE_OK_NO_SIGNIFICANT_JAMMING                = 0x01,
    JP_GNSS_JAMMING_STATE_WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK  = 0x02,
    JP_GNSS_JAMMING_STATE_CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX = 0x03
} jp_gnss_jamming_state_t;

typedef enum
{
    JP_GNSS_ANTENNA_STATUS_INIT      = 0x00,
    JP_GNSS_ANTENNA_STATUS_DONT_KNOW = 0x01,
    JP_GNSS_ANTENNA_STATUS_OK        = 0x02,
    JP_GNSS_ANTENNA_STATUS_SHORT     = 0x03,
    JP_GNSS_ANTENNA_STATUS_OPEN      = 0x04
} jp_gnss_antenna_status_t;

typedef enum
{
    JP_GNSS_ANTENNA_POWER_OFF       = 0x00,
    JP_GNSS_ANTENNA_POWER_ON        = 0x01,
    JP_GNSS_ANTENNA_POWER_DONT_KNOW = 0x02
} jp_gnss_antenna_power_t;

typedef struct
{
    float geometric;
    float position;
    float time;
    float vertical;
    float horizontal;
    float northing;
    float easting;
} jp_gnss_dilution_over_precision_t;

typedef struct
{
    uint8_t hh;
    uint8_t mm;
    uint8_t ss;
    bool valid;
    int32_t accuracy;
} jp_gnss_utc_time_t;

typedef struct
{
    uint8_t day;
    uint8_t month;
    uint16_t year;
    bool valid;
} jp_gnss_date_t;

typedef struct
{
    jp_gnss_fix_quality_t quality;
    jp_gnss_fix_status_t status;
    jp_gnss_fix_type_t type;

    jp_gnss_utc_time_t utc;
    jp_gnss_date_t date;

    float altitude;
    float altitude_msl;

    double latitude;
    double longitude;

    float speed_over_ground;
    float speed_accuracy;
    float heading;
    float heading_accuracy;

    uint8_t visible_satellites;

    float horizontal_accuracy;
    float vertical_accuracy;
} jp_gnss_position_velocity_time_t;

typedef struct
{
    float lat;
    float lon;
    float radius;
} jp_gnss_geofence_t;

typedef struct
{
    uint8_t pio_pin_number;
    jp_gnss_pio_pin_polarity_t pin_polarity;
    bool pio_enabled;
    uint8_t confidence_level;
    jp_gnss_geofence_t geofences[UBLOX_MAX_GEOFENCES];
    uint8_t geofence_count;
} jp_gnss_geofencing_cfg_t;

typedef struct
{
    uint32_t iTOW;
    jp_gnss_geofencing_status_t geofencing_status;
    uint8_t number_of_geofences;
    jp_gnss_geofence_status_t combined_state;
    jp_gnss_geofence_status_t geofences_status[UBLOX_MAX_GEOFENCES];
} jp_gnss_geofencing_nav_t;

typedef struct
{
    jp_gnss_geofencing_cfg_t cfg;
    jp_gnss_geofencing_nav_t nav;
} jp_gnss_geofencing_t;

typedef struct
{
    jp_gnss_rf_band_t id;
    jp_gnss_jamming_state_t jamming_state;
    jp_gnss_antenna_status_t antenna_status;
    jp_gnss_antenna_power_t antenna_power;
    uint32_t post_status;
    uint16_t noise_per_ms;
    float agc_monitor;
    float cw_interference_suppression_level;

    int8_t ofs_i;
    uint8_t mag_i;
    int8_t ofs_q;
    uint8_t mag_q;
} jp_gnss_rf_block_t;

typedef struct
{
    uint32_t frequency;
    float pulse_width;
} jp_gnss_pulse_t;

typedef struct
{
    bool active;
    jp_gnss_pulse_t fixed_pulse;
    bool has_pulse_when_no_fix;
    jp_gnss_pulse_t pulse_when_no_fix;
    jp_gnss_timepulse_polarity_t polarity;
} jp_gnss_timepulse_pin_config_t;

typedef struct
{
    jp_gnss_geofence_t geofences[UBLOX_MAX_GEOFENCES];
    uint8_t geofence_count;
    uint8_t confidence_level;
    bool has_pin_polarity;
    jp_gnss_pio_pin_polarity_t pin_polarity;
} jp_gnss_geofencing_config_t;

typedef struct
{
    uint16_t measurement_rate_hz;
    jp_gnss_dynamic_model_t dynamic_model;
    jp_gnss_timepulse_pin_config_t timepulse_pin_config;
    bool has_geofencing;
    jp_gnss_geofencing_config_t geofencing;
} jp_gnss_gnss_config_t;

typedef struct
{
    jp_gnss_dilution_over_precision_t dop;
    jp_gnss_position_velocity_time_t pvt;
    jp_gnss_geofencing_t geofencing;
    uint8_t num_rf_blocks;
    jp_gnss_rf_block_t rf_blocks[UBLOX_MAX_RF_BLOCKS];
} jp_gnss_navigation_t;

typedef struct
{
    uint8_t* data;
    uint32_t size;
} jp_gnss_rtcm3_frame_t;

typedef struct
{
    jp_gnss_rtcm3_frame_t* frames;
    uint32_t count;
} jp_gnss_rtk_corrections_t;

typedef struct jp_gnss_hat jp_gnss_hat_t;

jp_gnss_hat_t* jp_gnss_hat_create(void);
void jp_gnss_hat_destroy(jp_gnss_hat_t* hat);
bool jp_gnss_hat_start(jp_gnss_hat_t* hat,
    const jp_gnss_gnss_config_t* config);
bool jp_gnss_hat_wait_and_get_fresh_navigation(jp_gnss_hat_t* hat,
    jp_gnss_navigation_t* navigation);
bool jp_gnss_hat_get_navigation(jp_gnss_hat_t* hat,
    jp_gnss_navigation_t* navigation);
bool jp_gnss_hat_enable_timepulse(jp_gnss_hat_t* hat);
void jp_gnss_hat_disable_timepulse(jp_gnss_hat_t* hat);
bool jp_gnss_hat_start_forward_for_gpsd(jp_gnss_hat_t* hat);
void jp_gnss_hat_stop_forward_for_gpsd(jp_gnss_hat_t* hat);
void jp_gnss_hat_join_forward_for_gpsd(jp_gnss_hat_t* hat);
const char* jp_gnss_hat_get_gpsd_device_path(jp_gnss_hat_t* hat);
void jp_gnss_hat_hard_reset_cold_start(jp_gnss_hat_t* hat);
void jp_gnss_hat_soft_reset_hot_start(jp_gnss_hat_t* hat);
void jp_gnss_hat_timepulse(jp_gnss_hat_t* hat);

void jp_gnss_gnss_config_init(jp_gnss_gnss_config_t* config);
bool jp_gnss_gnss_config_add_geofence(jp_gnss_gnss_config_t* config,
    jp_gnss_geofence_t geofence);

jp_gnss_rtk_corrections_t* jp_gnss_rtk_get_full_corrections(
    jp_gnss_hat_t* hat);
jp_gnss_rtk_corrections_t* jp_gnss_rtk_get_tiny_corrections(
    jp_gnss_hat_t* hat);
jp_gnss_rtcm3_frame_t* jp_gnss_rtk_get_rtcm3_frame(jp_gnss_hat_t* hat,
    uint16_t id);
bool jp_gnss_rtk_apply_corrections(jp_gnss_hat_t* hat,
    const jp_gnss_rtcm3_frame_t* frames, uint32_t count);
void jp_gnss_rtk_corrections_free(jp_gnss_rtk_corrections_t* corrections);
void jp_gnss_rtcm3_frame_free(jp_gnss_rtcm3_frame_t* frame);

const char* jp_gnss_fix_quality_to_string(jp_gnss_fix_quality_t quality);
const char* jp_gnss_fix_status_to_string(jp_gnss_fix_status_t status);
const char* jp_gnss_fix_type_to_string(jp_gnss_fix_type_t type);
const char* jp_gnss_jamming_state_to_string(jp_gnss_jamming_state_t state);
const char* jp_gnss_antenna_status_to_string(jp_gnss_antenna_status_t status);
const char* jp_gnss_antenna_power_to_string(jp_gnss_antenna_power_t power);
const char* jp_gnss_rf_band_to_string(jp_gnss_rf_band_t band);
const char* jp_gnss_geofencing_status_to_string(
    jp_gnss_geofencing_status_t status);
const char* jp_gnss_geofence_status_to_string(jp_gnss_geofence_status_t status);

const char* jp_gnss_utc_time_iso8601(
    const jp_gnss_position_velocity_time_t* pvt);

#ifdef __cplusplus
}
#endif

#endif  // GNSS_HAT_H_
