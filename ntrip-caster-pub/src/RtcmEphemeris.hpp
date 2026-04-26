/*
 * Jimmy Paputto 2026
 *
 * Decoders for RTCM3 ephemeris messages, in the four constellations
 * relevant to a u-blox F9P base station:
 *
 *   - 1019  GPS  LNAV
 *   - 1044  QZSS LNAV
 *   - 1046  Galileo I/NAV (E1)
 *   - 1042  BeiDou D1/D2
 *
 * GLONASS (1020) is NOT decoded here — its ephemeris model is a
 * numerically-integrated state vector, not a Keplerian set, and
 * deserves its own propagator.
 *
 * Each decoder fills a `KeplerEph` shared with `SatPos.hpp`.
 *
 * Header-only.
 */

#ifndef NTRIP_CASTER_RTCM_EPHEMERIS_HPP_
#define NTRIP_CASTER_RTCM_EPHEMERIS_HPP_

#include <cmath>
#include <cstdint>
#include <optional>

#include "RtcmArp.hpp"   // detail::bitsU / bitsS
#include "SatPos.hpp"

namespace JimmyPaputto
{

    namespace ephem_detail
    {
        // Power-of-two scale factors used by RTCM bit fields.
        inline double pow2(int n) { return std::ldexp(1.0, n); }

        // semicircles → radians
        inline double sc2rad(double semi) { return semi * M_PI; }
    }

    /// Read message type from the first 12 bits of an RTCM3 payload.
    inline uint16_t rtcmMsgType(const uint8_t* payload)
    {
        return static_cast<uint16_t>(detail::bitsU(payload, 0, 12));
    }

    /// Decode RTCM 1019 (GPS LNAV ephemeris).  Layout per
    /// RTCM 10403.3 §3.5.5.  Returns std::nullopt if the payload is
    /// too short.
    inline std::optional<KeplerEph>
    decodeRtcm1019(const uint8_t* payload, size_t payloadBits)
    {
        constexpr size_t kRequired = 12 + 488;
        if (payloadBits < kRequired) return std::nullopt;

        using namespace ephem_detail;
        size_t p = 12; // skip msg type
        auto u = [&](size_t n) { auto v = detail::bitsU(payload, p, n); p += n; return v; };
        auto s = [&](size_t n) { auto v = detail::bitsS(payload, p, n); p += n; return v; };

        KeplerEph e;
        e.gnss        = GnssCode::GPS;
        e.svId        = static_cast<uint8_t>(u(6));
        e.weekNumber  = static_cast<uint16_t>(u(10));
        (void)u(4);                                  // URA
        (void)u(2);                                  // CODE on L2
        e.idot        = sc2rad(static_cast<double>(s(14)) * pow2(-43));
        (void)u(8);                                  // IODE
        e.toc         = static_cast<double>(u(16)) * 16.0;
        e.af2         = static_cast<double>(s(8))  * pow2(-55);
        e.af1         = static_cast<double>(s(16)) * pow2(-43);
        e.af0         = static_cast<double>(s(22)) * pow2(-31);
        (void)u(10);                                 // IODC
        e.Crs         = static_cast<double>(s(16)) * pow2(-5);
        e.deltaN      = sc2rad(static_cast<double>(s(16)) * pow2(-43));
        e.M0          = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cuc         = static_cast<double>(s(16)) * pow2(-29);
        e.e           = static_cast<double>(u(32)) * pow2(-33);
        e.Cus         = static_cast<double>(s(16)) * pow2(-29);
        e.sqrtA       = static_cast<double>(u(32)) * pow2(-19);
        e.toe         = static_cast<double>(u(16)) * 16.0;
        e.Cic         = static_cast<double>(s(16)) * pow2(-29);
        e.OMEGA0      = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cis         = static_cast<double>(s(16)) * pow2(-29);
        e.i0          = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Crc         = static_cast<double>(s(16)) * pow2(-5);
        e.omega       = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.OMEGAdot    = sc2rad(static_cast<double>(s(24)) * pow2(-43));
        (void)s(8);                                  // tGD
        e.health      = static_cast<uint8_t>(u(6));
        // remaining bits (L2P, fit-interval) ignored
        return e;
    }

    /// Decode RTCM 1044 (QZSS LNAV ephemeris).  RTCM 10403.3 §3.5.7.
    /// QZSS PRN is reported as 1-10 in the wire format and mapped to
    /// the conventional 193-202 range.
    inline std::optional<KeplerEph>
    decodeRtcm1044(const uint8_t* payload, size_t payloadBits)
    {
        constexpr size_t kRequired = 12 + 485;
        if (payloadBits < kRequired) return std::nullopt;

        using namespace ephem_detail;
        size_t p = 12;
        auto u = [&](size_t n) { auto v = detail::bitsU(payload, p, n); p += n; return v; };
        auto s = [&](size_t n) { auto v = detail::bitsS(payload, p, n); p += n; return v; };

        KeplerEph e;
        e.gnss = GnssCode::QZSS;
        e.svId = static_cast<uint8_t>(192 + u(4));
        e.toc       = static_cast<double>(u(16)) * 16.0;
        e.af2       = static_cast<double>(s(8))  * pow2(-55);
        e.af1       = static_cast<double>(s(16)) * pow2(-43);
        e.af0       = static_cast<double>(s(22)) * pow2(-31);
        (void)u(8);                                  // IODE
        e.Crs       = static_cast<double>(s(16)) * pow2(-5);
        e.deltaN    = sc2rad(static_cast<double>(s(16)) * pow2(-43));
        e.M0        = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cuc       = static_cast<double>(s(16)) * pow2(-29);
        e.e         = static_cast<double>(u(32)) * pow2(-33);
        e.Cus       = static_cast<double>(s(16)) * pow2(-29);
        e.sqrtA     = static_cast<double>(u(32)) * pow2(-19);
        e.toe       = static_cast<double>(u(16)) * 16.0;
        e.Cic       = static_cast<double>(s(16)) * pow2(-29);
        e.OMEGA0    = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cis       = static_cast<double>(s(16)) * pow2(-29);
        e.i0        = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Crc       = static_cast<double>(s(16)) * pow2(-5);
        e.omega     = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.OMEGAdot  = sc2rad(static_cast<double>(s(24)) * pow2(-43));
        e.idot      = sc2rad(static_cast<double>(s(14)) * pow2(-43));
        (void)u(2);                                  // CODE on L2
        e.weekNumber = static_cast<uint16_t>(u(10));
        (void)u(4);                                  // URA
        e.health     = static_cast<uint8_t>(u(6));
        // remaining bits (TGD, IODC, fit-interval) ignored
        return e;
    }

    /// Decode RTCM 1046 (Galileo I/NAV ephemeris).  RTCM 10403.3 §3.5.10.
    inline std::optional<KeplerEph>
    decodeRtcm1046(const uint8_t* payload, size_t payloadBits)
    {
        constexpr size_t kRequired = 12 + 504;
        if (payloadBits < kRequired) return std::nullopt;

        using namespace ephem_detail;
        size_t p = 12;
        auto u = [&](size_t n) { auto v = detail::bitsU(payload, p, n); p += n; return v; };
        auto s = [&](size_t n) { auto v = detail::bitsS(payload, p, n); p += n; return v; };

        KeplerEph e;
        e.gnss        = GnssCode::GAL;
        e.svId        = static_cast<uint8_t>(u(6));
        e.weekNumber  = static_cast<uint16_t>(u(12));
        (void)u(10);                                 // IODnav
        (void)u(8);                                  // SISA
        e.idot        = sc2rad(static_cast<double>(s(14)) * pow2(-43));
        e.toc         = static_cast<double>(u(14)) * 60.0;
        e.af2         = static_cast<double>(s(6))  * pow2(-59);
        e.af1         = static_cast<double>(s(21)) * pow2(-46);
        e.af0         = static_cast<double>(s(31)) * pow2(-34);
        e.Crs         = static_cast<double>(s(16)) * pow2(-5);
        e.deltaN      = sc2rad(static_cast<double>(s(16)) * pow2(-43));
        e.M0          = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cuc         = static_cast<double>(s(16)) * pow2(-29);
        e.e           = static_cast<double>(u(32)) * pow2(-33);
        e.Cus         = static_cast<double>(s(16)) * pow2(-29);
        e.sqrtA       = static_cast<double>(u(32)) * pow2(-19);
        e.toe         = static_cast<double>(u(14)) * 60.0;
        e.Cic         = static_cast<double>(s(16)) * pow2(-29);
        e.OMEGA0      = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cis         = static_cast<double>(s(16)) * pow2(-29);
        e.i0          = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Crc         = static_cast<double>(s(16)) * pow2(-5);
        e.omega       = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.OMEGAdot    = sc2rad(static_cast<double>(s(24)) * pow2(-43));
        (void)s(10);                                 // BGD E1/E5a
        e.health      = static_cast<uint8_t>(u(2));  // OS Health Status (E1B)
        // remaining: validity + reserved
        return e;
    }

    /// Decode RTCM 1042 (BeiDou D1/D2 ephemeris).  RTCM 10403.3 §3.5.6.
    inline std::optional<KeplerEph>
    decodeRtcm1042(const uint8_t* payload, size_t payloadBits)
    {
        constexpr size_t kRequired = 12 + 499;
        if (payloadBits < kRequired) return std::nullopt;

        using namespace ephem_detail;
        size_t p = 12;
        auto u = [&](size_t n) { auto v = detail::bitsU(payload, p, n); p += n; return v; };
        auto s = [&](size_t n) { auto v = detail::bitsS(payload, p, n); p += n; return v; };

        KeplerEph e;
        e.gnss        = GnssCode::BDS;
        e.svId        = static_cast<uint8_t>(u(6));
        e.weekNumber  = static_cast<uint16_t>(u(13));
        (void)u(4);                                  // URA index
        e.idot        = sc2rad(static_cast<double>(s(14)) * pow2(-43));
        (void)u(5);                                  // AODE
        e.toc         = static_cast<double>(u(17)) * 8.0;
        e.af2         = static_cast<double>(s(11)) * pow2(-66);
        e.af1         = static_cast<double>(s(22)) * pow2(-50);
        e.af0         = static_cast<double>(s(24)) * pow2(-33);
        (void)u(5);                                  // AODC
        e.Crs         = static_cast<double>(s(18)) * pow2(-6);
        e.deltaN      = sc2rad(static_cast<double>(s(16)) * pow2(-43));
        e.M0          = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cuc         = static_cast<double>(s(18)) * pow2(-31);
        e.e           = static_cast<double>(u(32)) * pow2(-33);
        e.Cus         = static_cast<double>(s(18)) * pow2(-31);
        e.sqrtA       = static_cast<double>(u(32)) * pow2(-19);
        e.toe         = static_cast<double>(u(17)) * 8.0;
        e.Cic         = static_cast<double>(s(18)) * pow2(-31);
        e.OMEGA0      = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Cis         = static_cast<double>(s(18)) * pow2(-31);
        e.i0          = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.Crc         = static_cast<double>(s(18)) * pow2(-6);
        e.omega       = sc2rad(static_cast<double>(s(32)) * pow2(-31));
        e.OMEGAdot    = sc2rad(static_cast<double>(s(24)) * pow2(-43));
        (void)s(10);                                 // TGD1
        (void)s(10);                                 // TGD2
        e.health      = static_cast<uint8_t>(u(1));
        return e;
    }

    /// Compute current GPS time-of-week (seconds, 0..604800) from
    /// system clock.  Sky-plot accuracy needs ~1 s; we use UTC + the
    /// current GPS-UTC leap-second count (18 since 2017-01-01).
    inline double currentGpsTowSeconds(uint64_t unixMs, int leapSecs = 18)
    {
        // GPS epoch: 1980-01-06T00:00:00 UTC = unix 315964800.
        constexpr uint64_t kGpsEpochUnix = 315964800ULL;
        uint64_t unixSec = unixMs / 1000;
        if (unixSec < kGpsEpochUnix) return 0.0;
        uint64_t gpsSec = unixSec - kGpsEpochUnix + static_cast<uint64_t>(leapSecs);
        double tow = static_cast<double>(gpsSec % 604800ULL)
                   + static_cast<double>(unixMs % 1000) / 1000.0;
        return tow;
    }

    /// Per-GNSS time-of-week relative to the constellation's own week
    /// origin.  Galileo time = GPS time, QZSS = GPS, BDS lags GPS by
    /// 14 seconds.  GLONASS not handled.
    inline double gnssTowFromGpsTow(double gpsTow, uint8_t gnss)
    {
        if (gnss == GnssCode::BDS)
        {
            double t = gpsTow - 14.0;
            if (t < 0.0) t += 604800.0;
            return t;
        }
        return gpsTow;
    }

}

#endif // NTRIP_CASTER_RTCM_EPHEMERIS_HPP_
