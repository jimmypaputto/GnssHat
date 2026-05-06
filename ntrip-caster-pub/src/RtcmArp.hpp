/*
 * Jimmy Paputto 2026
 *
 * Minimal RTCM3 1005/1006 (Stationary RTK Reference Station ARP) decoder.
 * Used by NtripCaster to extract base-station position from the relayed
 * stream so it can advertise it in the sourcetable.
 */

#ifndef NTRIP_CASTER_RTCM_ARP_HPP_
#define NTRIP_CASTER_RTCM_ARP_HPP_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace JimmyPaputto
{

    struct RtcmArpPosition
    {
        double latitudeDeg;
        double longitudeDeg;
        double heightMeters;
    };

    namespace detail
    {
        inline uint64_t bitsU(const uint8_t *buf, size_t pos, size_t nbits)
        {
            uint64_t v = 0;
            for (size_t i = 0; i < nbits; ++i)
            {
                size_t bit = pos + i;
                v = (v << 1) | ((buf[bit / 8] >> (7 - (bit % 8))) & 1u);
            }
            return v;
        }

        inline int64_t bitsS(const uint8_t *buf, size_t pos, size_t nbits)
        {
            uint64_t u = bitsU(buf, pos, nbits);
            if (u & (1ULL << (nbits - 1)))
                u |= ~((1ULL << nbits) - 1ULL);
            return static_cast<int64_t>(u);
        }

        inline void ecefToLla(double x, double y, double z,
                              double &latDeg, double &lonDeg, double &h)
        {
            constexpr double a   = 6378137.0;
            constexpr double f   = 1.0 / 298.257223563;
            constexpr double b   = a * (1.0 - f);
            constexpr double e2  = f * (2.0 - f);
            constexpr double ep2 = e2 / (1.0 - e2);

            double p   = std::sqrt(x * x + y * y);
            double th  = std::atan2(a * z, b * p);
            double lon = std::atan2(y, x);
            double lat = std::atan2(z + ep2 * b * std::pow(std::sin(th), 3),
                                    p - e2 * a * std::pow(std::cos(th), 3));
            double N   = a / std::sqrt(1.0 - e2 * std::sin(lat) * std::sin(lat));
            h          = p / std::cos(lat) - N;

            constexpr double radToDeg = 57.29577951308232;
            latDeg = lat * radToDeg;
            lonDeg = lon * radToDeg;
        }
    } // namespace detail

    /// Try to decode a single RTCM3 frame as message 1005 or 1006.
    /// `frame` must point to the 3-byte header; `len` is the full frame length
    /// including the 3-byte CRC trailer.
    /// Returns std::nullopt if the frame is not 1005/1006 or is malformed.
    inline std::optional<RtcmArpPosition>
    decodeRtcm1005(const uint8_t *frame, size_t len)
    {
        if (len < 25 || frame[0] != 0xD3)
            return std::nullopt;

        const uint8_t *payload  = frame + 3;
        size_t        payloadBits = static_cast<size_t>(
            ((frame[1] & 0x03) << 8) | frame[2]) * 8;

        // Need at least 152 bits for 1005, 168 for 1006.
        if (payloadBits < 152)
            return std::nullopt;

        uint16_t msgType =
            static_cast<uint16_t>(detail::bitsU(payload, 0, 12));
        if (msgType != 1005 && msgType != 1006)
            return std::nullopt;

        // ECEF X/Y/Z fields are at bit offsets 34, 74, 114 within the payload.
        // Units: 0.0001 m (0.1 mm).
        constexpr double scale = 1e-4;
        double x = static_cast<double>(detail::bitsS(payload, 34, 38)) * scale;
        double y = static_cast<double>(detail::bitsS(payload, 74, 38)) * scale;
        double z = static_cast<double>(detail::bitsS(payload, 114, 38)) * scale;

        // Sanity: must be roughly on/near Earth's surface.
        double r = std::sqrt(x * x + y * y + z * z);
        if (r < 6.0e6 || r > 6.6e6)
            return std::nullopt;

        RtcmArpPosition pos{};
        detail::ecefToLla(x, y, z, pos.latitudeDeg, pos.longitudeDeg,
                          pos.heightMeters);
        return pos;
    }

}

#endif // NTRIP_CASTER_RTCM_ARP_HPP_
