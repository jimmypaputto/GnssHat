/*
 * Jimmy Paputto 2026
 */

#ifndef NTRIP_LOG_HPP_
#define NTRIP_LOG_HPP_

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <string>

namespace JimmyPaputto
{

    enum class ENtripLogLevel : uint8_t
    {
        Error   = 0,
        Warning = 1,
        Info    = 2,
        Debug   = 3,
    };

    using NtripLogCallback = std::function<void(ENtripLogLevel, const std::string &)>;

    /// Mixin that adds configurable logging to NTRIP classes.
    /// By default no callback is set, so all messages are silently discarded.
    class NtripLoggable
    {
    public:
        void setLogCallback(NtripLogCallback cb) { logCallback_ = std::move(cb); }
        void setLogLevel(ENtripLogLevel level) { minLogLevel_ = level; }

    protected:
        void log(ENtripLogLevel level, const char *fmt, ...) const
            __attribute__((format(printf, 3, 4)))
        {
            if (!logCallback_ || level > minLogLevel_)
                return;

            char buf[512];
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(buf, sizeof(buf), fmt, ap);
            va_end(ap);

            logCallback_(level, buf);
        }

    private:
        NtripLogCallback logCallback_;
        ENtripLogLevel minLogLevel_ = ENtripLogLevel::Info;
    };

}

#endif // NTRIP_LOG_HPP_
