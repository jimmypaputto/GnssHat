/*
 * Jimmy Paputto 2026
 */

#ifndef SYSTEM_HEALTH_HPP_
#define SYSTEM_HEALTH_HPP_

#include <cstdint>


namespace JimmyPaputto
{

// UBX-MON-SYS bootType field (UBX-23006991 §3.14.13).
enum class EBootType : std::uint8_t
{
    Unknown        = 0,
    ColdStart      = 1,
    Watchdog       = 2,
    HardwareReset  = 3,
    HardwareBackup = 4,
    SoftwareBackup = 5,
    SoftwareReset  = 6,
    VioFail        = 7,
    VddXFail       = 8,
    VddRfFail      = 9,
    VCoreHighFail  = 10,
    SystemReset    = 11
};

// Snapshot of UBX-MON-SYS — current system performance information.
//
// cpuLoadMax/memUsageMax/ioUsageMax are peaks since the last MON-SYS report,
// so they are only meaningful when the message is emitted at 1 Hz (which we
// configure in Startup).
struct SystemHealth
{
    bool      valid          = false;  // set true once a frame is received
    uint8_t   msgVersion     = 0;
    EBootType bootType       = EBootType::Unknown;
    uint8_t   cpuLoad        = 0;      // [%]
    uint8_t   cpuLoadMax     = 0;      // [%]
    uint8_t   memUsage       = 0;      // [%]
    uint8_t   memUsageMax    = 0;      // [%]
    uint8_t   ioUsage        = 0;      // [%]
    uint8_t   ioUsageMax     = 0;      // [%]
    uint32_t  runTime        = 0;      // [s] since last restart
    uint16_t  noticeCount    = 0;
    uint16_t  warnCount      = 0;
    uint16_t  errorCount     = 0;
    int8_t    temperatureC   = 0;      // [°C], accuracy ±2°C per spec
};

}  // JimmyPaputto

#endif  // SYSTEM_HEALTH_HPP_
