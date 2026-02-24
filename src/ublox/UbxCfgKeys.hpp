/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_UBX_CFG_KEYS_HPP_
#define JIMMY_PAPUTTO_UBX_CFG_KEYS_HPP_

#include <cstdint>


namespace JimmyPaputto::UbxCfgKeys
{

constexpr uint32_t CFG_SPI_MAXFF           = 0x20640001;
constexpr uint32_t CFG_SPI_CPOLARITY       = 0x10640002;
constexpr uint32_t CFG_SPI_CPHASE          = 0x10640003;
constexpr uint32_t CFG_SPI_EXTENDEDTIMEOUT = 0x10640005;
constexpr uint32_t CFG_SPI_ENABLED         = 0x10640006;

constexpr uint32_t CFG_SPIINPROT_UBX    = 0x10790001;
constexpr uint32_t CFG_SPIINPROT_NMEA   = 0x10790002;
constexpr uint32_t CFG_SPIINPROT_RTCM3X = 0x10790004;
constexpr uint32_t CFG_SPIINPROT_SPARTN = 0x10790005;

constexpr uint32_t CFG_SPIOUTPROT_UBX    = 0x107a0001;
constexpr uint32_t CFG_SPIOUTPROT_NMEA   = 0x107a0002;
constexpr uint32_t CFG_SPIOUTPROT_RTCM3X = 0x107a0004;

constexpr uint32_t CFG_UART1_BAUDRATE = 0x40520001;
constexpr uint32_t CFG_UART1_STOPBITS = 0x20520002;
constexpr uint32_t CFG_UART1_DATABITS = 0x20520003;
constexpr uint32_t CFG_UART1_PARITY   = 0x20520004;
constexpr uint32_t CFG_UART1_ENABLED  = 0x10520005;

constexpr uint32_t CFG_UART1OUTPROT_UBX  = 0x10740001;
constexpr uint32_t CFG_UART1OUTPROT_NMEA = 0x10740002;

constexpr uint32_t CFG_UART2_BAUDRATE = 0x40530001;
constexpr uint32_t CFG_UART2_STOPBITS = 0x20530002;
constexpr uint32_t CFG_UART2_DATABITS = 0x20530003;
constexpr uint32_t CFG_UART2_PARITY   = 0x20530004;
constexpr uint32_t CFG_UART2_ENABLED  = 0x10530005;

constexpr uint32_t CFG_UART2INPROT_RTCM3X = 0x10750004;

constexpr uint32_t CFG_UART2OUTPROT_UBX  = 0x10760001;
constexpr uint32_t CFG_UART2OUTPROT_NMEA = 0x10760002;
constexpr uint32_t CFG_UART2OUTPROT_RTCM3X = 0x10760004;

constexpr uint32_t CFG_TXREADY_ENABLED   = 0x10a20001;
constexpr uint32_t CFG_TXREADY_POLARITY  = 0x10a20002;
constexpr uint32_t CFG_TXREADY_PIN       = 0x20a20003;
constexpr uint32_t CFG_TXREADY_THRESHOLD = 0x30a20004;
constexpr uint32_t CFG_TXREADY_INTERFACE = 0x20a20005;

constexpr uint32_t CFG_RATE_MEAS    = 0x30210001;
constexpr uint32_t CFG_RATE_NAV     = 0x30210002;
constexpr uint32_t CFG_RATE_TIMEREF = 0x20210003;

constexpr uint32_t CFG_NAVSPG_DYNMODEL = 0x20110021;

constexpr uint32_t CFG_TP_TP1_ENA          = 0x10050007;
constexpr uint32_t CFG_TP_PULSE_DEF        = 0x20050023;
constexpr uint32_t CFG_TP_PULSE_LENGTH_DEF = 0x20050030;
constexpr uint32_t CFG_TP_FREQ_TP1         = 0x40050024;
constexpr uint32_t CFG_TP_FREQ_LOCK_TP1    = 0x40050025;
constexpr uint32_t CFG_TP_DUTY_TP1         = 0x5005002a;
constexpr uint32_t CFG_TP_DUTY_LOCK_TP1    = 0x5005002b;
constexpr uint32_t CFG_TP_ANT_CABLEDELAY   = 0x30050001;
constexpr uint32_t CFG_TP_USER_DELAY_TP1   = 0x40050006;
constexpr uint32_t CFG_TP_POL_TP1          = 0x1005000b;
constexpr uint32_t CFG_TP_TIMEGRID_TP1     = 0x2005000c;

// CFG-MSGOUT UART1
constexpr uint32_t CFG_MSGOUT_UBX_MON_RF_UART1  = 0x2091035a;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_DOP_UART1 = 0x20910039;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_PVT_UART1 = 0x20910007;

// CFG-MSGOUT SPI
constexpr uint32_t CFG_MSGOUT_UBX_MON_RF_SPI       = 0x2091035d;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_DOP_SPI      = 0x2091003c;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_PVT_SPI      = 0x2091000a;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_SAT_SPI      = 0x20910019;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_GEOFENCE_SPI = 0x209100a5;

constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1005_UART2 = 0x209102bf;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1074_UART2 = 0x20910360;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1077_UART2 = 0x209102ce;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1084_UART2 = 0x20910365;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1087_UART2 = 0x209102d3;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1094_UART2 = 0x2091036a;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1097_UART2 = 0x2091031a;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1124_UART2 = 0x2091036f;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1127_UART2 = 0x209102d8;
constexpr uint32_t CFG_MSGOUT_RTCM_3X_TYPE1230_UART2 = 0x20910305;

// CFG-GEOFENCE
constexpr uint32_t CFG_GEOFENCE_CONFLVL     = 0x20240011;
constexpr uint32_t CFG_GEOFENCE_USE_PIO     = 0x10240012;
constexpr uint32_t CFG_GEOFENCE_PINPOL      = 0x20240013;
constexpr uint32_t CFG_GEOFENCE_PIN         = 0x20240014;
constexpr uint32_t CFG_GEOFENCE_USE_FENCE1  = 0x10240020;
constexpr uint32_t CFG_GEOFENCE_FENCE1_LAT  = 0x40240021;
constexpr uint32_t CFG_GEOFENCE_FENCE1_LON  = 0x40240022;
constexpr uint32_t CFG_GEOFENCE_FENCE1_RAD  = 0x40240023;
constexpr uint32_t CFG_GEOFENCE_USE_FENCE2  = 0x10240030;
constexpr uint32_t CFG_GEOFENCE_FENCE2_LAT  = 0x40240031;
constexpr uint32_t CFG_GEOFENCE_FENCE2_LON  = 0x40240032;
constexpr uint32_t CFG_GEOFENCE_FENCE2_RAD  = 0x40240033;
constexpr uint32_t CFG_GEOFENCE_USE_FENCE3  = 0x10240040;
constexpr uint32_t CFG_GEOFENCE_FENCE3_LAT  = 0x40240041;
constexpr uint32_t CFG_GEOFENCE_FENCE3_LON  = 0x40240042;
constexpr uint32_t CFG_GEOFENCE_FENCE3_RAD  = 0x40240043;
constexpr uint32_t CFG_GEOFENCE_USE_FENCE4  = 0x10240050;
constexpr uint32_t CFG_GEOFENCE_FENCE4_LAT  = 0x40240051;
constexpr uint32_t CFG_GEOFENCE_FENCE4_LON  = 0x40240052;
constexpr uint32_t CFG_GEOFENCE_FENCE4_RAD  = 0x40240053;

constexpr uint32_t CFG_TMODE_MODE           = 0x20030001;
constexpr uint32_t CFG_TMODE_POS_TYPE       = 0x20030002;
constexpr uint32_t CFG_TMODE_ECEF_X         = 0x40030003;
constexpr uint32_t CFG_TMODE_ECEF_Y         = 0x40030004;
constexpr uint32_t CFG_TMODE_ECEF_Z         = 0x40030005;
constexpr uint32_t CFG_TMODE_ECEF_X_HP      = 0x20030006;
constexpr uint32_t CFG_TMODE_ECEF_Y_HP      = 0x20030007;
constexpr uint32_t CFG_TMODE_ECEF_Z_HP      = 0x20030008;
constexpr uint32_t CFG_TMODE_LAT            = 0x40030009;
constexpr uint32_t CFG_TMODE_LON            = 0x4003000a;
constexpr uint32_t CFG_TMODE_HEIGHT         = 0x4003000b;
constexpr uint32_t CFG_TMODE_LAT_HP         = 0x2003000c;
constexpr uint32_t CFG_TMODE_LON_HP         = 0x2003000d;
constexpr uint32_t CFG_TMODE_HEIGHT_HP      = 0x2003000e;
constexpr uint32_t CFG_TMODE_FIXED_POS_ACC  = 0x4003000f;
constexpr uint32_t CFG_TMODE_SVIN_MIN_DUR   = 0x40030010;
constexpr uint32_t CFG_TMODE_SVIN_ACC_LIMIT = 0x40030011;

}  // JimmyPaputto::UbxCfgKeys

#endif  // JIMMY_PAPUTTO_UBX_CFG_KEYS_HPP_
