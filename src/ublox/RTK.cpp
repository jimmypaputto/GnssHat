/*
 * Jimmy Paputto 2025
 */

#include "RTK.hpp"
#include "RtkFactory.hpp"


namespace JimmyPaputto
{

class Base : public IBase
{
public:
    explicit Base(const Rtcm3Store& rtcm3Store);
    ~Base() override = default;

    std::vector<std::vector<uint8_t>> getFullCorrections() override;
    std::vector<std::vector<uint8_t>> getTinyCorrections() override;
    std::vector<uint8_t> getRtcm3Frame(const uint16_t id) override;

private:
    const std::vector<uint16_t> tinyCorrectionIds = {
        1005, 1074, 1084, 1094, 1124, 1230
    };
    const std::vector<uint16_t> fullCorrectionIds = {
        1005, 1077, 1087, 1097, 1127, 1230
    };
    const Rtcm3Store& rtcm3Store_;
};

class Rover : public IRover
{
public:
    explicit Rover(Rtcm3Store& rtcm3Store);
    ~Rover() override = default;

    void applyCorrections(
        const std::vector<std::vector<uint8_t>>& corrections) override;

private:
    Rtcm3Store& rtcm3Store_;
};

class Rtk : public IRtk
{
public:
    Rtk(Rtcm3Store& rtcm3Store, const GnssConfig& config);
    ~Rtk() override = default;

    IBase* base() override;
    IRover* rover() override;

private:
    std::unique_ptr<IBase> base_;
    std::unique_ptr<IRover> rover_;
};

IRtk* RtkFactory::create(Rtcm3Store& rtcm3Store, const GnssConfig& config)
{
    return new Rtk(rtcm3Store, config);
}

Base::Base(const Rtcm3Store& rtcm3Store)
:   rtcm3Store_(rtcm3Store)
{
}

std::vector<std::vector<uint8_t>> Base::getFullCorrections()
{
    return rtcm3Store_.getFrames(fullCorrectionIds);
}

std::vector<std::vector<uint8_t>> Base::getTinyCorrections()
{
    return rtcm3Store_.getFrames(tinyCorrectionIds);
}

std::vector<uint8_t> Base::getRtcm3Frame(const uint16_t id)
{
    return rtcm3Store_.getFrame(id);
}

Rover::Rover(Rtcm3Store& rtcm3Store)
:   rtcm3Store_(rtcm3Store)
{
}

void Rover::applyCorrections(
    const std::vector<std::vector<uint8_t>>& corrections)
{
    rtcm3Store_.updateFramesAndNotify(corrections);
}

Rtk::Rtk(Rtcm3Store& rtcm3Store, const GnssConfig& config)
:   base_(nullptr)
{
    if (config.rtk == std::nullopt)
        return;

    if (config.rtk->mode == ERtkMode::Base)
    {
        base_ = std::make_unique<Base>(rtcm3Store);
    }
    else if (config.rtk->mode == ERtkMode::Rover)
    {
        rover_ = std::make_unique<Rover>(rtcm3Store);
    }
}

IBase* Rtk::base()
{
    return base_ != nullptr ? base_.get() : nullptr;
}

IRover* Rtk::rover()
{
    return rover_ != nullptr ? rover_.get() : nullptr;
}

}  // JimmyPaputto
