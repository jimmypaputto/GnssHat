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
    {UbxCfgKeys::CFG_SPIINPROT_NMEA,   4},
    {UbxCfgKeys::CFG_SPIINPROT_RTCM3X, 1},
    {UbxCfgKeys::CFG_SPIINPROT_SPARTN, 1},

    {UbxCfgKeys::CFG_SPIOUTPROT_UBX,    1}, 
    {UbxCfgKeys::CFG_SPIOUTPROT_NMEA,   4},
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
};

}  // JimmyPaputto::ubxmsg
