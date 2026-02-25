/*
 * Jimmy Paputto 2025
 */

#include "UBX_CFG_VALGET.hpp"
#include "ublox/UbxCfgKeys.hpp"


namespace JimmyPaputto::ubxmsg
{

std::unordered_map<uint32_t, uint8_t> ConfigKeySizeMap::keySizes_ = {
    {UbxCfgKeys::CFG_SPI_MAXFF,           1}, 
    {UbxCfgKeys::CFG_SPI_CPOLARITY,       1},
    {UbxCfgKeys::CFG_SPI_CPHASE,          1},
    {UbxCfgKeys::CFG_SPI_EXTENDEDTIMEOUT, 1},
    {UbxCfgKeys::CFG_SPI_ENABLED,         1},

    {UbxCfgKeys::CFG_SPIINPROT_UBX,    1},
    {UbxCfgKeys::CFG_SPIINPROT_NMEA,   1},
    {UbxCfgKeys::CFG_SPIINPROT_RTCM3X, 1},
    {UbxCfgKeys::CFG_SPIINPROT_SPARTN, 1},

    {UbxCfgKeys::CFG_SPIOUTPROT_UBX,    1},
    {UbxCfgKeys::CFG_SPIOUTPROT_NMEA,   1},
    {UbxCfgKeys::CFG_SPIOUTPROT_RTCM3X, 1},

    {UbxCfgKeys::CFG_UART1_ENABLED,  1}, 
    {UbxCfgKeys::CFG_UART1_BAUDRATE, 4},
    {UbxCfgKeys::CFG_UART1_DATABITS, 1},
    {UbxCfgKeys::CFG_UART1_PARITY,   1},
    {UbxCfgKeys::CFG_UART1_STOPBITS, 1},

    {UbxCfgKeys::CFG_UART1OUTPROT_UBX,  1},
    {UbxCfgKeys::CFG_UART1OUTPROT_NMEA, 1},

    {UbxCfgKeys::CFG_UART2_ENABLED,  1}, 
    {UbxCfgKeys::CFG_UART2_BAUDRATE, 4},
    {UbxCfgKeys::CFG_UART2_DATABITS, 1},
    {UbxCfgKeys::CFG_UART2_PARITY,   1},
    {UbxCfgKeys::CFG_UART2_STOPBITS, 1},

    {UbxCfgKeys::CFG_UART2INPROT_RTCM3X, 1},

    {UbxCfgKeys::CFG_UART2OUTPROT_UBX,    1},
    {UbxCfgKeys::CFG_UART2OUTPROT_NMEA,   1},
    {UbxCfgKeys::CFG_UART2OUTPROT_RTCM3X, 1},

    {UbxCfgKeys::CFG_TXREADY_ENABLED,   1},
    {UbxCfgKeys::CFG_TXREADY_POLARITY,  1},
    {UbxCfgKeys::CFG_TXREADY_PIN,       1},
    {UbxCfgKeys::CFG_TXREADY_THRESHOLD, 2},
    {UbxCfgKeys::CFG_TXREADY_INTERFACE, 1},

    {UbxCfgKeys::CFG_RATE_MEAS,    2},
    {UbxCfgKeys::CFG_RATE_NAV,     2},
    {UbxCfgKeys::CFG_RATE_TIMEREF, 1},

    {UbxCfgKeys::CFG_NAVSPG_DYNMODEL, 1},

    {UbxCfgKeys::CFG_TP_TP1_ENA,          1},
    {UbxCfgKeys::CFG_TP_PULSE_DEF,        1},
    {UbxCfgKeys::CFG_TP_PULSE_LENGTH_DEF, 1},
    {UbxCfgKeys::CFG_TP_FREQ_TP1,         4},
    {UbxCfgKeys::CFG_TP_FREQ_LOCK_TP1,    4},
    {UbxCfgKeys::CFG_TP_DUTY_TP1,         8},
    {UbxCfgKeys::CFG_TP_DUTY_LOCK_TP1,    8},
    {UbxCfgKeys::CFG_TP_ANT_CABLEDELAY,   2},
    {UbxCfgKeys::CFG_TP_USER_DELAY_TP1,   4},
    {UbxCfgKeys::CFG_TP_POL_TP1,          1},
    {UbxCfgKeys::CFG_TP_TIMEGRID_TP1,     1},

    {UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_UART1,  1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_UART1, 1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_UART1, 1},

    {UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_SPI,       1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_SPI,      1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_SPI,      1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_SAT_SPI,      1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_GEOFENCE_SPI, 1},

    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1005_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1074_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1077_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1084_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1087_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1094_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1097_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1124_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1127_UART2, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM_3X_TYPE1230_UART2, 1},

    {UbxCfgKeys::CFG_GEOFENCE_CONFLVL,     1},
    {UbxCfgKeys::CFG_GEOFENCE_USE_PIO,     1},
    {UbxCfgKeys::CFG_GEOFENCE_PINPOL,      1},
    {UbxCfgKeys::CFG_GEOFENCE_PIN,         1},
    {UbxCfgKeys::CFG_GEOFENCE_USE_FENCE1,  1},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE1_LAT,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE1_LON,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE1_RAD,  4},
    {UbxCfgKeys::CFG_GEOFENCE_USE_FENCE2,  1},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE2_LAT,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE2_LON,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE2_RAD,  4},
    {UbxCfgKeys::CFG_GEOFENCE_USE_FENCE3,  1},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE3_LAT,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE3_LON,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE3_RAD,  4},
    {UbxCfgKeys::CFG_GEOFENCE_USE_FENCE4,  1},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE4_LAT,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE4_LON,  4},
    {UbxCfgKeys::CFG_GEOFENCE_FENCE4_RAD,  4},

    {UbxCfgKeys::CFG_TMODE_MODE,           1},
    {UbxCfgKeys::CFG_TMODE_POS_TYPE,       1},
    {UbxCfgKeys::CFG_TMODE_ECEF_X,         4},
    {UbxCfgKeys::CFG_TMODE_ECEF_Y,         4},
    {UbxCfgKeys::CFG_TMODE_ECEF_Z,         4},
    {UbxCfgKeys::CFG_TMODE_ECEF_X_HP,      1},
    {UbxCfgKeys::CFG_TMODE_ECEF_Y_HP,      1},
    {UbxCfgKeys::CFG_TMODE_ECEF_Z_HP,      1},
    {UbxCfgKeys::CFG_TMODE_LAT,            4},
    {UbxCfgKeys::CFG_TMODE_LON,            4},
    {UbxCfgKeys::CFG_TMODE_HEIGHT,         4},
    {UbxCfgKeys::CFG_TMODE_LAT_HP,         1},
    {UbxCfgKeys::CFG_TMODE_LON_HP,         1},
    {UbxCfgKeys::CFG_TMODE_HEIGHT_HP,      1},
    {UbxCfgKeys::CFG_TMODE_FIXED_POS_ACC,  4},
    {UbxCfgKeys::CFG_TMODE_SVIN_MIN_DUR,   4},
    {UbxCfgKeys::CFG_TMODE_SVIN_ACC_LIMIT, 4},
};

}  // JimmyPaputto::ubxmsg
