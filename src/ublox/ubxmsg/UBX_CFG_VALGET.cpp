/*
 * Jimmy Paputto 2025
 */

#include "UBX_CFG_VALGET.hpp"
#include "ublox/UbxCfgKeys.hpp"


namespace JimmyPaputto::ubxmsg
{

std::unordered_map<uint32_t, uint8_t> ConfigKeySizeMap::keySizes_ = {
    {UbxCfgKeys::CFG_UART1_ENABLED,  1}, 
    {UbxCfgKeys::CFG_UART1_BAUDRATE, 4},
    {UbxCfgKeys::CFG_UART1_DATABITS, 1},
    {UbxCfgKeys::CFG_UART1_PARITY,   1},
    {UbxCfgKeys::CFG_UART1_STOPBITS, 1},

    {UbxCfgKeys::CFG_UART2_ENABLED,  1}, 
    {UbxCfgKeys::CFG_UART2_BAUDRATE, 4},
    {UbxCfgKeys::CFG_UART2_DATABITS, 1},
    {UbxCfgKeys::CFG_UART2_PARITY,   1},
    {UbxCfgKeys::CFG_UART2_STOPBITS, 1},

    {UbxCfgKeys::CFG_TXREADY_ENABLED,   1},
    {UbxCfgKeys::CFG_TXREADY_POLARITY,  1},
    {UbxCfgKeys::CFG_TXREADY_PIN,       1},
    {UbxCfgKeys::CFG_TXREADY_THRESHOLD, 2},
    {UbxCfgKeys::CFG_TXREADY_INTERFACE, 1},

    {UbxCfgKeys::CFG_UART1OUTPROT_UBX,  1},
    {UbxCfgKeys::CFG_UART1OUTPROT_NMEA, 1},

    {UbxCfgKeys::CFG_MSGOUT_UBX_MON_RF_UART1,  1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_DOP_UART1, 1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_NAV_PVT_UART1, 1},

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

    {UbxCfgKeys::CFG_TMODE_MODE,           1},
    {UbxCfgKeys::CFG_TMODE_SVIN_MIN_DUR,   4},
    {UbxCfgKeys::CFG_TMODE_SVIN_ACC_LIMIT, 4},

    // Protocol Output Configuration (U1)
    {UbxCfgKeys::CFG_MSGOUT_UBX_SPI, 1},
    {UbxCfgKeys::CFG_MSGOUT_NMEA_SPI, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM3_SPI, 1},
    {UbxCfgKeys::CFG_MSGOUT_UBX_UART1, 1},
    {UbxCfgKeys::CFG_MSGOUT_NMEA_UART1, 1},
    {UbxCfgKeys::CFG_MSGOUT_RTCM3_UART1, 1},
    
    // Protocol Input Configuration (U1)
    {UbxCfgKeys::CFG_MSGIN_UBX_SPI, 1},
    {UbxCfgKeys::CFG_MSGIN_NMEA_SPI, 1},
    {UbxCfgKeys::CFG_MSGIN_RTCM3_SPI, 1},
    {UbxCfgKeys::CFG_MSGIN_UBX_UART1, 1},
    {UbxCfgKeys::CFG_MSGIN_NMEA_UART1, 1},
    {UbxCfgKeys::CFG_MSGIN_RTCM3_UART1, 1},
    
    // Rate Configuration (U2)
    {UbxCfgKeys::CFG_RATE_MEAS, 2},
    {UbxCfgKeys::CFG_RATE_NAV, 2},  
    {UbxCfgKeys::CFG_RATE_TIMEREF, 2},
    
    // Navigation Configuration (U2)
    {UbxCfgKeys::CFG_NAVSPG_DYNMODEL, 2},
    
    // Timepulse Configuration
    {UbxCfgKeys::CFG_TP_TP1, 1},                 // U1
    {UbxCfgKeys::CFG_TP_PULSE_DEF, 2},           // U2
    {UbxCfgKeys::CFG_TP_PULSE_LENGTH_DEF, 2},    // U2
    {UbxCfgKeys::CFG_TP_ANT_CABLEDELAY, 2},      // I2
    {UbxCfgKeys::CFG_TP_PERIOD_TP1, 4},          // U4
    {UbxCfgKeys::CFG_TP_PERIOD_LOCK_TP1, 4},     // U4
    {UbxCfgKeys::CFG_TP_LEN_TP1, 4},             // U4
    {UbxCfgKeys::CFG_TP_LEN_LOCK_TP1, 4},        // U4
    {UbxCfgKeys::CFG_TP_USER_DELAY_TP1, 4},      // I4
    {UbxCfgKeys::CFG_TP_SYNC_GNSS_TP1, 1},       // U1
    {UbxCfgKeys::CFG_TP_USE_LOCKED_TP1, 1},      // U1
    {UbxCfgKeys::CFG_TP_ALIGN_TO_TOW_TP1, 1},    // U1
    {UbxCfgKeys::CFG_TP_POL_TP1, 1},             // U1
    {UbxCfgKeys::CFG_TP_TIMEGRID_TP1, 1},        // U1
};

}  // JimmyPaputto::ubxmsg