/*
 * Jimmy Paputto 2021
 */

#ifndef GNSS_HPP_
#define GNSS_HPP_

#include "common/GenericSingleton.hpp"
#include "common/JPGuard.hpp"

#include "ublox/Navigation.hpp"
#include "ublox/SystemHealth.hpp"
#include "ublox/TimeMark.hpp"

#include <optional>
#include <string>


namespace JimmyPaputto
{

class Gnss: public GenericSingleton<Gnss>
{
public:
    explicit Gnss();

    void dop(const DilutionOverPrecision& dop);
    void pvt(const PositionVelocityTime& pvt);
    void geofencingCfg(const Geofencing::Cfg& cfg);
    void geofencingNav(const Geofencing::Nav& nav);
    void rfBlocks(const std::vector<RfBlock>& rfBlocks);
    void rfBlocksSpectrumData(const std::vector<RfBlockSpectrumData>& rfBlocksSpectrumData);
    void satellites(const std::vector<SatelliteInfo>& satellites);

    void monVer(const std::string& swVersion, const std::string& hwVersion,
                const std::vector<std::string>& extensions);
    std::string swVersion() const;
    std::string hwVersion() const;
    std::vector<std::string> monVerExtensions() const;

    void systemHealth(const SystemHealth& systemHealth);
    SystemHealth systemHealth() const;

    void timeMark(const TimeMark& timeMark);
    std::optional<TimeMark> timeMark() const;

    bool lock() const;
    void unlock() const;

    Navigation navigation() const;

private:
    Navigation navigation_;
    std::optional<TimeMark> timeMark_;
    std::string swVersion_;
    std::string hwVersion_;
    std::vector<std::string> monVerExtensions_;
    SystemHealth systemHealth_;
    mutable JPGuard xSemaphore_;
};

}  // JimmyPaputto

#endif  // GNSS_HPP_
