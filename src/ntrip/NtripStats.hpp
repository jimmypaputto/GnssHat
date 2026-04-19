/*
 * Jimmy Paputto 2026
 *
 * Connection statistics for NtripCaster / NtripClient.
 */

#ifndef NTRIP_STATS_HPP_
#define NTRIP_STATS_HPP_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

namespace JimmyPaputto
{

    struct NtripStats
    {
        uint64_t bytesTx = 0;
        uint64_t bytesRx = 0;
        uint64_t framesTx = 0;
        uint64_t framesRx = 0;
        uint64_t uptimeMs = 0;
        uint64_t lastFrameAgeMs = 0;
        double avgInterFrameMs = 0.0;
        double maxInterFrameMs = 0.0;
        std::map<uint16_t, uint32_t> messageTypeCounts;
    };

    /// Mixin that tracks NTRIP statistics.  Thread-safe.
    class NtripStatsTracker
    {
    public:
        NtripStats getStats() const
        {
            NtripStats s;
            s.bytesTx = bytesTx_.load(std::memory_order_relaxed);
            s.bytesRx = bytesRx_.load(std::memory_order_relaxed);
            s.framesTx = framesTx_.load(std::memory_order_relaxed);
            s.framesRx = framesRx_.load(std::memory_order_relaxed);

            auto now = Clock::now();
            if (startTime_ != TimePoint{})
                s.uptimeMs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - startTime_)
                        .count());

            auto last = lastFrameTime_.load(std::memory_order_relaxed);
            if (last != TimePoint{})
                s.lastFrameAgeMs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last)
                        .count());

            {
                std::lock_guard<std::mutex> lk(statsMutex_);
                s.messageTypeCounts = messageTypeCounts_;
                s.avgInterFrameMs = avgInterFrameMs_;
                s.maxInterFrameMs = maxInterFrameMs_;
            }
            return s;
        }

    protected:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        void statsStart()
        {
            startTime_ = Clock::now();
        }

        void statsReset()
        {
            bytesTx_ = 0;
            bytesRx_ = 0;
            framesTx_ = 0;
            framesRx_ = 0;
            startTime_ = TimePoint{};
            lastFrameTime_ = TimePoint{};
            std::lock_guard<std::mutex> lk(statsMutex_);
            messageTypeCounts_.clear();
            avgInterFrameMs_ = 0.0;
            maxInterFrameMs_ = 0.0;
            interFrameCount_ = 0;
            interFrameSum_ = 0.0;
        }

        void statsRecordTx(size_t bytes, size_t numFrames)
        {
            bytesTx_.fetch_add(bytes, std::memory_order_relaxed);
            framesTx_.fetch_add(numFrames, std::memory_order_relaxed);
            recordFrameTimestamp();
        }

        /// Record raw RTCM3 stream bytes, scanning for 0xD3 preambles to
        /// extract message types and count frames.
        void statsRecordTxRaw(const uint8_t* data, size_t len)
        {
            bytesTx_.fetch_add(len, std::memory_order_relaxed);

            size_t frames = 0;
            {
                std::lock_guard<std::mutex> lk(statsMutex_);
                for (size_t i = 0; i + 5 <= len; )
                {
                    if (data[i] == 0xD3)
                    {
                        uint16_t payloadLen =
                            (static_cast<uint16_t>(data[i + 1] & 0x03) << 8) |
                            static_cast<uint16_t>(data[i + 2]);
                        size_t frameLen = 3 + payloadLen + 3; // header + payload + CRC

                        uint16_t msgType =
                            (static_cast<uint16_t>(data[i + 3]) << 4) |
                            (static_cast<uint16_t>(data[i + 4]) >> 4);
                        messageTypeCounts_[msgType]++;
                        ++frames;

                        if (i + frameLen <= len)
                            i += frameLen;
                        else
                            break;
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            framesTx_.fetch_add(frames, std::memory_order_relaxed);
            if (frames > 0)
                recordFrameTimestamp();
        }

        void statsRecordRx(size_t bytes)
        {
            bytesRx_.fetch_add(bytes, std::memory_order_relaxed);
        }

        void statsRecordFrame(const uint8_t* data, size_t len)
        {
            framesRx_.fetch_add(1, std::memory_order_relaxed);
            recordFrameTimestamp();

            // Extract RTCM3 message type from first 2 bytes of payload
            if (len >= 5)
            {
                uint16_t msgType = (static_cast<uint16_t>(data[3]) << 4) |
                                   (static_cast<uint16_t>(data[4]) >> 4);
                std::lock_guard<std::mutex> lk(statsMutex_);
                messageTypeCounts_[msgType]++;
            }
        }

        void statsRecordTxFrames(
            const std::vector<std::vector<uint8_t>>& frames)
        {
            size_t totalBytes = 0;
            for (const auto& f : frames)
                totalBytes += f.size();
            framesTx_.fetch_add(frames.size(), std::memory_order_relaxed);
            bytesTx_.fetch_add(totalBytes, std::memory_order_relaxed);
            recordFrameTimestamp();

            std::lock_guard<std::mutex> lk(statsMutex_);
            for (const auto& f : frames)
            {
                if (f.size() >= 5)
                {
                    uint16_t msgType =
                        (static_cast<uint16_t>(f[3]) << 4) |
                        (static_cast<uint16_t>(f[4]) >> 4);
                    messageTypeCounts_[msgType]++;
                }
            }
        }

    private:
        void recordFrameTimestamp()
        {
            auto now = Clock::now();
            auto prev = lastFrameTime_.exchange(now,
                                                std::memory_order_relaxed);
            if (prev != TimePoint{})
            {
                double deltaMs =
                    std::chrono::duration<double, std::milli>(now - prev)
                        .count();
                std::lock_guard<std::mutex> lk(statsMutex_);
                interFrameCount_++;
                interFrameSum_ += deltaMs;
                avgInterFrameMs_ = interFrameSum_ / interFrameCount_;
                if (deltaMs > maxInterFrameMs_)
                    maxInterFrameMs_ = deltaMs;
            }
        }

        std::atomic<uint64_t> bytesTx_{0};
        std::atomic<uint64_t> bytesRx_{0};
        std::atomic<uint64_t> framesTx_{0};
        std::atomic<uint64_t> framesRx_{0};
        TimePoint startTime_{};
        std::atomic<TimePoint> lastFrameTime_{TimePoint{}};

        mutable std::mutex statsMutex_;
        std::map<uint16_t, uint32_t> messageTypeCounts_;
        double avgInterFrameMs_ = 0.0;
        double maxInterFrameMs_ = 0.0;
        uint64_t interFrameCount_ = 0;
        double interFrameSum_ = 0.0;
    };

}
#endif // NTRIP_STATS_HPP_
