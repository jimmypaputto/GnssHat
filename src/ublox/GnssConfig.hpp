/*
 * Jimmy Paputto 2022
 */

#ifndef JIMMY_PAPUTTO_GNSS_CONFIG_HPP_
#define JIMMY_PAPUTTO_GNSS_CONFIG_HPP_

#include <optional>
#include <vector>

#include "EDynamicModel.hpp"
#include "Geofence.hpp"
#include "RtkConfig.hpp"
#include "TimepulsePinConfig.hpp"
#include "TimingConfig.hpp"


namespace JimmyPaputto
{

struct GnssConfig
{
    uint16_t measurementRate_Hz;

    EDynamicModel dynamicModel;

    TimepulsePinConfig timepulsePinConfig;

    struct Geofencing
    {
        std::vector<Geofence> geofences;
        uint8_t confidenceLevel;
        std::optional<EPioPinPolarity> pioPinPolarity;
    };
    std::optional<Geofencing> geofencing{};

    std::optional<RtkConfig> rtk{};

    std::optional<TimingConfig> timing{};

    struct NavigationFilters
    {
        // u-blox CFG-NAVSPG-INFIL_* keys. Each field is optional so callers
        // can override a single filter and leave the rest at the receiver's
        // current (typically factory-default) setting.
        std::optional<uint8_t> minSvs{};        // CFG-NAVSPG-INFIL_MINSVS  (U1, count)
        std::optional<uint8_t> maxSvs{};        // CFG-NAVSPG-INFIL_MAXSVS  (U1, count)
        std::optional<uint8_t> minCno_dBHz{};   // CFG-NAVSPG-INFIL_MINCNO  (U1, dBHz)
        std::optional<int8_t>  minElev_deg{};   // CFG-NAVSPG-INFIL_MINELEV (I1, deg, signed)
        std::optional<uint8_t> nCnoThrs{};      // CFG-NAVSPG-INFIL_NCNOTHRS (U1, count)
        std::optional<uint8_t> cnoThrs_dBHz{};  // CFG-NAVSPG-INFIL_CNOTHRS (U1, dBHz)

        // CFG-NAVSPG-FIXMODE — 1 = 2D-only, 2 = 3D-only, 3 = Auto 2D/3D.
        enum class FixMode : uint8_t
        {
            Only2D = 1,
            Only3D = 2,
            Auto   = 3,
        };
        std::optional<FixMode> fixMode{};

        // CFG-NAVSPG-OUTFIL_* — solution-output masks. A candidate fix is
        // flagged invalid when any enabled threshold is exceeded. Values
        // mirror the receiver's native encoding so the wire format is
        // transparent.
        std::optional<uint16_t> pdopMask_x10{};  // U2, 0.1 DOP units
        std::optional<uint16_t> tdopMask_x10{};  // U2, 0.1 DOP units
        std::optional<uint16_t> pAccMask_m{};    // U2, metres
        std::optional<uint16_t> tAccMask_m{};    // U2, metres
    };
    std::optional<NavigationFilters> navigationFilters{};

    bool saveToFlash{false};
};

bool checkMeasurementRate(const uint16_t measurementRate);
bool checkTimepulsePinConfig(const TimepulsePinConfig& timepulsePinConfig);
bool checkGeofencing(const std::optional<GnssConfig::Geofencing>& geofencing);
bool checkTiming(const std::optional<TimingConfig>& timing);
bool checkNavigationFilters(
    const std::optional<GnssConfig::NavigationFilters>& filters);

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_GNSS_CONFIG_HPP_
