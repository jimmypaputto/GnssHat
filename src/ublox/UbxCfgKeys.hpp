/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_UBX_CFG_KEYS_HPP_
#define JIMMY_PAPUTTO_UBX_CFG_KEYS_HPP_

#include <cstdint>


namespace JimmyPaputto::UbxCfgKeys
{

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

constexpr uint32_t CFG_MSGOUT_UBX_MON_RF_UART1  = 0x2091035a;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_DOP_UART1 = 0x20910039;
constexpr uint32_t CFG_MSGOUT_UBX_NAV_PVT_UART1 = 0x20910007;

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

constexpr uint32_t CFG_TMODE_MODE           = 0x20030001;
constexpr uint32_t CFG_TMODE_SVIN_MIN_DUR   = 0x40030010;
constexpr uint32_t CFG_TMODE_SVIN_ACC_LIMIT = 0x40030011;

constexpr uint32_t CFG_MSGOUT_UBX_SPI = 0x10720001;      // UBX protocol on SPI (U1)
constexpr uint32_t CFG_MSGOUT_NMEA_SPI = 0x10720002;     // NMEA protocol on SPI (U1)
constexpr uint32_t CFG_MSGOUT_RTCM3_SPI = 0x10720003;    // RTCM3 protocol on SPI (U1)

constexpr uint32_t CFG_MSGOUT_UBX_UART1 = 0x10730001;    // UBX protocol on UART1 (U1)
constexpr uint32_t CFG_MSGOUT_NMEA_UART1 = 0x10730002;   // NMEA protocol on UART1 (U1)
constexpr uint32_t CFG_MSGOUT_RTCM3_UART1 = 0x10730003;  // RTCM3 protocol on UART1 (U1)

constexpr uint32_t CFG_MSGIN_UBX_SPI = 0x10750001;       // UBX input on SPI (U1)
constexpr uint32_t CFG_MSGIN_NMEA_SPI = 0x10750002;      // NMEA input on SPI (U1)
constexpr uint32_t CFG_MSGIN_RTCM3_SPI = 0x10750003;     // RTCM3 input on SPI (U1)

constexpr uint32_t CFG_MSGIN_UBX_UART1 = 0x10760001;     // UBX input on UART1 (U1)
constexpr uint32_t CFG_MSGIN_NMEA_UART1 = 0x10760002;    // NMEA input on UART1 (U1)
constexpr uint32_t CFG_MSGIN_RTCM3_UART1 = 0x10760003;   // RTCM3 input on UART1 (U1)

constexpr uint32_t CFG_RATE_MEAS = 0x30210001;           // Measurement rate [ms] (U2)
constexpr uint32_t CFG_RATE_NAV = 0x30210002;            // Navigation rate [cycles] (U2)  
constexpr uint32_t CFG_RATE_TIMEREF = 0x20210003;        // Time reference (U2: 0=UTC, 1=GPS)

constexpr uint32_t CFG_NAVSPG_DYNMODEL = 0x20110021;

constexpr uint32_t CFG_TP_PULSE_DEF = 0x20050023;        // Timepulse definition (U2: 0=period, 1=freq)
constexpr uint32_t CFG_TP_PULSE_LENGTH_DEF = 0x20050030; // Pulse length definition (U2: 0=ratio, 1=length)
constexpr uint32_t CFG_TP_ANT_CABLEDELAY = 0x10050001;   // Antenna cable delay [ns] (I2)
constexpr uint32_t CFG_TP_PERIOD_TP1 = 0x40050002;       // Timepulse 1 period [us] (U4)
constexpr uint32_t CFG_TP_PERIOD_LOCK_TP1 = 0x40050003;  // Timepulse 1 period when locked [us] (U4)
constexpr uint32_t CFG_TP_LEN_TP1 = 0x40050004;          // Timepulse 1 pulse length [us] (U4) 
constexpr uint32_t CFG_TP_LEN_LOCK_TP1 = 0x40050005;     // Timepulse 1 pulse length when locked [us] (U4)
constexpr uint32_t CFG_TP_USER_DELAY_TP1 = 0x40050006;   // Timepulse 1 user delay [ns] (I4)
constexpr uint32_t CFG_TP_TP1 = 0x10050007;              // Timepulse 1 enabled (U1)
constexpr uint32_t CFG_TP_SYNC_GNSS_TP1 = 0x10050008;    // Sync timepulse 1 to GNSS (U1)
constexpr uint32_t CFG_TP_USE_LOCKED_TP1 = 0x10050009;   // Use locked parameters when possible (U1)
constexpr uint32_t CFG_TP_ALIGN_TO_TOW_TP1 = 0x1005000A; // Align pulse to top of second (U1)
constexpr uint32_t CFG_TP_POL_TP1 = 0x1005000B;          // Timepulse 1 polarity (U1: 0=falling, 1=rising)
constexpr uint32_t CFG_TP_TIMEGRID_TP1 = 0x1005000C;     // Timepulse 1 time grid (U1: 0=UTC, 1=GPS)

}  // JimmyPaputto::UbxCfgKeys

#endif  // JIMMY_PAPUTTO_UBX_CFG_KEYS_HPP_
