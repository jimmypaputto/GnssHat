/*
 * Jimmy Paputto 2025
 */

#ifndef JP_RTK_HPP_
#define JP_RTK_HPP_

#include <cstdint>
#include <vector>


namespace JimmyPaputto
{

class IBase
{
public:
    virtual std::vector<std::vector<uint8_t>> getFullCorrections() = 0;  // M7M
    virtual std::vector<std::vector<uint8_t>> getTinyCorrections() = 0;  // M4M
    virtual std::vector<uint8_t> getRtcm3Frame(const uint16_t id) = 0;

    virtual ~IBase() = default;
};

class IRover
{
public:
    virtual void applyCorrections(
        const std::vector<std::vector<uint8_t>>& corrections) = 0;

    virtual ~IRover() = default;
};

class IRtk
{
public:
    virtual IBase* base() = 0;
    virtual IRover* rover() = 0;

    virtual ~IRtk() = default;
};

}  // JimmyPaputto

#endif  // JP_RTK_HPP_
