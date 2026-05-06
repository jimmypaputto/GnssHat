/*
 * Jimmy Paputto 2026
 *
 * RTCM3 stream analyzer for ntrip-caster status page.
 *
 * Consumes raw bytes from a source connection, splits them into
 * RTCM3 frames (CRC-24Q validated), and aggregates a snapshot of
 * what the base station is currently transmitting:
 *
 *   - per-message-type frame counts and last-seen ages
 *   - latest base ARP (RTCM 1005 / 1006)
 *   - per-constellation visible-satellite mask (MSM4/5/6/7 headers)
 *
 * Header-only.  Reused bit helpers from RtcmArp.hpp.
 */

#ifndef NTRIP_CASTER_RTCM_ANALYZER_HPP_
#define NTRIP_CASTER_RTCM_ANALYZER_HPP_

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "RtcmArp.hpp"
#include "RtcmEphemeris.hpp"
#include "SatPos.hpp"

namespace JimmyPaputto
{

    /// Compact key identifying an ephemeris in the cache.
    struct SvKey
    {
        uint8_t gnss = 0;
        uint8_t svId = 0;
        bool operator<(const SvKey& o) const noexcept
        {
            return (static_cast<uint16_t>(gnss) << 8 | svId) <
                   (static_cast<uint16_t>(o.gnss) << 8 | o.svId);
        }
    };

    enum class EGnss : uint8_t
    {
        Unknown = 0,
        GPS,      // 107x
        GLONASS,  // 108x
        Galileo,  // 109x
        SBAS,     // 110x
        QZSS,     // 111x
        BeiDou,   // 112x
        NavIC,    // 113x
    };

    inline const char* gnssName(EGnss g)
    {
        switch (g)
        {
            case EGnss::GPS:     return "GPS";
            case EGnss::GLONASS: return "GLONASS";
            case EGnss::Galileo: return "Galileo";
            case EGnss::SBAS:    return "SBAS";
            case EGnss::QZSS:    return "QZSS";
            case EGnss::BeiDou:  return "BeiDou";
            case EGnss::NavIC:   return "NavIC";
            default:             return "Unknown";
        }
    }

    /// MSM message → constellation.  Returns Unknown for non-MSM types.
    /// MSM number is the low decimal digit (msgType % 10), valid 1..7.
    inline EGnss msmGnss(uint16_t msgType)
    {
        if (msgType < 1071 || msgType > 1137) return EGnss::Unknown;
        unsigned msmNum = msgType % 10;
        if (msmNum < 1 || msmNum > 7) return EGnss::Unknown;
        switch (msgType / 10)
        {
            case 107: return EGnss::GPS;
            case 108: return EGnss::GLONASS;
            case 109: return EGnss::Galileo;
            case 110: return EGnss::SBAS;
            case 111: return EGnss::QZSS;
            case 112: return EGnss::BeiDou;
            case 113: return EGnss::NavIC;
            default:  return EGnss::Unknown;
        }
    }

    inline uint16_t msmNumber(uint16_t msgType)
    {
        return static_cast<uint16_t>(msgType % 10);
    }

    /// Snapshot of one constellation's most recent MSM header.
    struct ConstellationView
    {
        EGnss     gnss          = EGnss::Unknown;
        uint16_t  lastMsgType   = 0;     // e.g. 1077, 1097
        uint16_t  msmNumber     = 0;     // 4 / 5 / 6 / 7
        uint64_t  satMask       = 0;     // DF394 — 64-bit MSB-first; bit i (0=MSB) = sat (i+1) present
        uint32_t  signalMask    = 0;     // DF395 — 32-bit MSB-first; bit i (0=MSB) = signal (i+1) present
        uint32_t  refStation    = 0;
        uint32_t  epochTimeMs   = 0;     // raw DF004/etc value (units depend on GNSS)
        uint64_t  lastSeenUnixMs = 0;    // monotonic — wall-clock unix ms

        /// Decoded list of SV indices (1..64) currently transmitted.
        std::vector<uint8_t> satIds() const
        {
            std::vector<uint8_t> out;
            for (int i = 0; i < 64; ++i)
                if (satMask & (1ULL << (63 - i)))
                    out.push_back(static_cast<uint8_t>(i + 1));
            return out;
        }

        std::vector<uint8_t> signalIds() const
        {
            std::vector<uint8_t> out;
            for (int i = 0; i < 32; ++i)
                if (signalMask & (1u << (31 - i)))
                    out.push_back(static_cast<uint8_t>(i + 1));
            return out;
        }
    };

    /// Snapshot of the live RTCM3 stream feeding the active mountpoint.
    struct RtcmSnapshot
    {
        std::optional<RtcmArpPosition> arp;          // base station ARP
        std::optional<double>          arpEcefX;     // raw ECEF, meters
        std::optional<double>          arpEcefY;
        std::optional<double>          arpEcefZ;
        std::map<uint16_t, uint32_t>   messageTypeCounts;
        std::map<uint16_t, uint64_t>   messageTypeLastMs; // last-seen unix ms
        std::map<EGnss, ConstellationView> constellations;
        uint64_t                       totalFrames = 0;
        uint64_t                       totalBytes  = 0;
        uint64_t                       lastFrameUnixMs = 0;
        std::map<SvKey, KeplerEph>     ephemerides;
    };

    namespace detail
    {
        // CRC-24Q (RTCM 10403.x).  Polynomial 0x1864CFB.
        inline uint32_t crc24q(const uint8_t* data, size_t length)
        {
            static const uint32_t kTable[256] = {
                0x000000,0x864CFB,0x8AD50D,0x0C99F6,0x93E6E1,0x15AA1A,0x1933EC,0x9F7F17,
                0xA18139,0x27CDC2,0x2B5434,0xAD18CF,0x3267D8,0xB42B23,0xB8B2D5,0x3EFE2E,
                0xC54E89,0x430272,0x4F9B84,0xC9D77F,0x56A868,0xD0E493,0xDC7D65,0x5A319E,
                0x64CFB0,0xE2834B,0xEE1ABD,0x685646,0xF72951,0x7165AA,0x7DFC5C,0xFBB0A7,
                0x0CD1E9,0x8A9D12,0x8604E4,0x00481F,0x9F3708,0x197BF3,0x15E205,0x93AEFE,
                0xAD50D0,0x2B1C2B,0x2785DD,0xA1C926,0x3EB631,0xB8FACA,0xB4633C,0x322FC7,
                0xC99F60,0x4FD39B,0x434A6D,0xC50696,0x5A7981,0xDC357A,0xD0AC8C,0x56E077,
                0x681E59,0xEE52A2,0xE2CB54,0x6487AF,0xFBF8B8,0x7DB443,0x712DB5,0xF7614E,
                0x19A3D2,0x9FEF29,0x9376DF,0x153A24,0x8A4533,0x0C09C8,0x00903E,0x86DCC5,
                0xB822EB,0x3E6E10,0x32F7E6,0xB4BB1D,0x2BC40A,0xAD88F1,0xA11107,0x275DFC,
                0xDCED5B,0x5AA1A0,0x563856,0xD074AD,0x4F0BBA,0xC94741,0xC5DEB7,0x43924C,
                0x7D6C62,0xFB2099,0xF7B96F,0x71F594,0xEE8A83,0x68C678,0x645F8E,0xE21375,
                0x15723B,0x933EC0,0x9FA736,0x19EBCD,0x8694DA,0x00D821,0x0C41D7,0x8A0D2C,
                0xB4F302,0x32BFF9,0x3E260F,0xB86AF4,0x2715E3,0xA15918,0xADC0EE,0x2B8C15,
                0xD03CB2,0x567049,0x5AE9BF,0xDCA544,0x43DA53,0xC596A8,0xC90F5E,0x4F43A5,
                0x71BD8B,0xF7F170,0xFB6886,0x7D247D,0xE25B6A,0x641791,0x688E67,0xEEC29C,
                0x3347A4,0xB50B5F,0xB992A9,0x3FDE52,0xA0A145,0x26EDBE,0x2A7448,0xAC38B3,
                0x92C69D,0x148A66,0x181390,0x9E5F6B,0x01207C,0x876C87,0x8BF571,0x0DB98A,
                0xF6092D,0x7045D6,0x7CDC20,0xFA90DB,0x65EFCC,0xE3A337,0xEF3AC1,0x69763A,
                0x578814,0xD1C4EF,0xDD5D19,0x5B11E2,0xC46EF5,0x42220E,0x4EBBF8,0xC8F703,
                0x3F964D,0xB9DAB6,0xB54340,0x330FBB,0xAC70AC,0x2A3C57,0x26A5A1,0xA0E95A,
                0x9E1774,0x185B8F,0x14C279,0x928E82,0x0DF195,0x8BBD6E,0x872498,0x016863,
                0xFAD8C4,0x7C943F,0x700DC9,0xF64132,0x693E25,0xEF72DE,0xE3EB28,0x65A7D3,
                0x5B59FD,0xDD1506,0xD18CF0,0x57C00B,0xC8BF1C,0x4EF3E7,0x426A11,0xC426EA,
                0x2AE476,0xACA88D,0xA0317B,0x267D80,0xB90297,0x3F4E6C,0x33D79A,0xB59B61,
                0x8B654F,0x0D29B4,0x01B042,0x87FCB9,0x1883AE,0x9ECF55,0x9256A3,0x141A58,
                0xEFAAFF,0x69E604,0x657FF2,0xE33309,0x7C4C1E,0xFA00E5,0xF69913,0x70D5E8,
                0x4E2BC6,0xC8673D,0xC4FECB,0x42B230,0xDDCD27,0x5B81DC,0x57182A,0xD154D1,
                0x26359F,0xA07964,0xACE092,0x2AAC69,0xB5D37E,0x339F85,0x3F0673,0xB94A88,
                0x87B4A6,0x01F85D,0x0D61AB,0x8B2D50,0x145247,0x921EBC,0x9E874A,0x18CBB1,
                0xE37B16,0x6537ED,0x69AE1B,0xEFE2E0,0x709DF7,0xF6D10C,0xFA48FA,0x7C0401,
                0x42FA2F,0xC4B6D4,0xC82F22,0x4E63D9,0xD11CCE,0x575035,0x5BC9C3,0xDD8538
            };
            uint32_t crc = 0;
            for (size_t i = 0; i < length; ++i)
                crc = ((crc << 8) & 0xFFFFFF) ^
                      kTable[((crc >> 16) ^ data[i]) & 0xFF];
            return crc;
        }
    } // namespace detail


    /// Stateful per-source RTCM3 stream parser.  feed() may be called
    /// from a single thread (the source-handler thread).  snapshot()
    /// is thread-safe and returns a shallow copy.
    class RtcmAnalyzer
    {
    public:
        /// Maximum bytes buffered while waiting for a complete frame.
        static constexpr size_t kMaxScanBytes = 32 * 1024;

        void feed(const uint8_t* data, size_t len)
        {
            std::lock_guard<std::mutex> lk(mtx_);
            scan_.insert(scan_.end(), data, data + len);
            snap_.totalBytes += len;

            size_t i = 0;
            while (i + 6 <= scan_.size())
            {
                if (scan_[i] != 0xD3)
                {
                    ++i;
                    continue;
                }
                uint16_t payloadLen =
                    (static_cast<uint16_t>(scan_[i + 1] & 0x03) << 8) |
                    static_cast<uint16_t>(scan_[i + 2]);
                size_t frameLen = 3 + payloadLen + 3;
                if (frameLen > 1029) // RTCM3 max frame size
                {
                    ++i;
                    continue;
                }
                if (i + frameLen > scan_.size())
                    break; // wait for more data

                const uint8_t* frame = scan_.data() + i;
                uint32_t got =
                    (static_cast<uint32_t>(frame[3 + payloadLen]) << 16) |
                    (static_cast<uint32_t>(frame[3 + payloadLen + 1]) << 8) |
                    static_cast<uint32_t>(frame[3 + payloadLen + 2]);
                uint32_t want = detail::crc24q(frame, 3 + payloadLen);
                if (got != want)
                {
                    ++i; // bad CRC — skip a byte and resync
                    continue;
                }

                processFrame(frame, frameLen);
                i += frameLen;
            }
            if (i > 0)
                scan_.erase(scan_.begin(), scan_.begin() + i);

            // Cap scan buffer to avoid unbounded growth on garbage.
            if (scan_.size() > kMaxScanBytes)
                scan_.erase(scan_.begin(),
                            scan_.begin() + (scan_.size() - kMaxScanBytes));
        }

        RtcmSnapshot snapshot() const
        {
            std::lock_guard<std::mutex> lk(mtx_);
            return snap_;
        }

        void reset()
        {
            std::lock_guard<std::mutex> lk(mtx_);
            scan_.clear();
            snap_ = {};
        }

    private:
        void processFrame(const uint8_t* frame, size_t frameLen)
        {
            const uint8_t* payload = frame + 3;
            size_t payloadBits = static_cast<size_t>(frameLen - 6) * 8;
            if (payloadBits < 12) return;

            uint16_t msgType =
                static_cast<uint16_t>(detail::bitsU(payload, 0, 12));
            uint64_t nowMs = unixNowMs();

            snap_.totalFrames++;
            snap_.lastFrameUnixMs = nowMs;
            snap_.messageTypeCounts[msgType]++;
            snap_.messageTypeLastMs[msgType] = nowMs;

            if (msgType == 1005 || msgType == 1006)
            {
                if (payloadBits >= 152)
                    decodeArp(payload, msgType);
            }
            else if (msgType == 1019 || msgType == 1042 ||
                     msgType == 1044 || msgType == 1046)
            {
                decodeEphemeris(payload, payloadBits, msgType, nowMs);
            }
            else
            {
                EGnss g = msmGnss(msgType);
                if (g != EGnss::Unknown)
                    decodeMsmHeader(payload, payloadBits, msgType, g, nowMs);
            }
        }

        void decodeEphemeris(const uint8_t* payload, size_t payloadBits,
                             uint16_t msgType, uint64_t nowMs)
        {
            std::optional<KeplerEph> eph;
            switch (msgType)
            {
                case 1019: eph = decodeRtcm1019(payload, payloadBits); break;
                case 1042: eph = decodeRtcm1042(payload, payloadBits); break;
                case 1044: eph = decodeRtcm1044(payload, payloadBits); break;
                case 1046: eph = decodeRtcm1046(payload, payloadBits); break;
                default:   return;
            }
            if (!eph) return;
            eph->receivedUnixMs = nowMs;
            SvKey k{ eph->gnss, eph->svId };
            snap_.ephemerides[k] = *eph;
        }

        void decodeArp(const uint8_t* payload, uint16_t msgType)
        {
            // ECEF X/Y/Z fields at bit offsets 34, 74, 114; 0.0001 m units.
            constexpr double scale = 1e-4;
            double x = static_cast<double>(detail::bitsS(payload, 34, 38)) * scale;
            double y = static_cast<double>(detail::bitsS(payload, 74, 38)) * scale;
            double z = static_cast<double>(detail::bitsS(payload, 114, 38)) * scale;
            double r = std::sqrt(x * x + y * y + z * z);
            if (r < 6.0e6 || r > 6.6e6)
                return;

            RtcmArpPosition pos{};
            detail::ecefToLla(x, y, z, pos.latitudeDeg, pos.longitudeDeg,
                              pos.heightMeters);
            snap_.arp = pos;
            snap_.arpEcefX = x;
            snap_.arpEcefY = y;
            snap_.arpEcefZ = z;
            (void)msgType;
        }

        // MSM header common layout (RTCM 10403.3 §3.5.1):
        //   DF002 msg type        12  → already consumed
        //   DF003 ref station id  12
        //   GNSS epoch time       30  (GPS/GAL/QZSS/BDS); GLO uses 27+3
        //   DF393 multi-msg bit    1
        //   DF409 IODS             3
        //   reserved               7
        //   DF001_7 clock-steer    2
        //   DF002_7 ext-clock      2
        //   DF411 smoothing ind    1
        //   DF412 smoothing intvl  3
        //   DF394 satellite mask  64
        //   DF395 signal mask     32
        //   DF396 cell mask       Nsat * Nsig (variable)
        // Total fixed header before satMask = 12+12+30+1+3+7+2+2+1+3 = 73
        // bits, plus the 12-bit msgType already consumed.
        void decodeMsmHeader(const uint8_t* payload, size_t payloadBits,
                             uint16_t msgType, EGnss gnss, uint64_t nowMs)
        {
            constexpr size_t kFixedHeaderBits = 12 + 12 + 30 + 1 + 3 + 7 +
                                                2 + 2 + 1 + 3 + 64 + 32;
            if (payloadBits < kFixedHeaderBits)
                return;

            size_t pos = 12; // skip msg type
            uint32_t refStation =
                static_cast<uint32_t>(detail::bitsU(payload, pos, 12)); pos += 12;
            uint32_t epochTimeMs =
                static_cast<uint32_t>(detail::bitsU(payload, pos, 30)); pos += 30;
            pos += 1 + 3 + 7 + 2 + 2 + 1 + 3; // skip flags
            uint64_t satMask    = detail::bitsU(payload, pos, 64); pos += 64;
            uint32_t signalMask =
                static_cast<uint32_t>(detail::bitsU(payload, pos, 32)); pos += 32;

            ConstellationView v;
            v.gnss           = gnss;
            v.lastMsgType    = msgType;
            v.msmNumber      = msmNumber(msgType);
            v.satMask        = satMask;
            v.signalMask     = signalMask;
            v.refStation     = refStation;
            v.epochTimeMs    = epochTimeMs;
            v.lastSeenUnixMs = nowMs;
            snap_.constellations[gnss] = v;
        }

        static uint64_t unixNowMs()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch())
                    .count());
        }

        mutable std::mutex   mtx_;
        std::vector<uint8_t> scan_;
        RtcmSnapshot         snap_;
    };

}

#endif // NTRIP_CASTER_RTCM_ANALYZER_HPP_
