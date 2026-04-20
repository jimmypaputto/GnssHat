/*
 * Jimmy Paputto 2026
 */

#include "common/BuildInfo.hpp"

#ifndef GNSSHAT_GIT_REVISION
#define GNSSHAT_GIT_REVISION "unknown"
#endif

#ifndef GNSSHAT_BUILD_NUMBER
#define GNSSHAT_BUILD_NUMBER 0
#endif

namespace JimmyPaputto
{
    namespace BuildInfo
    {
        std::string_view buildDate()
        {
            return __DATE__;
        }

        std::string_view buildTime()
        {
            return __TIME__;
        }

        std::string_view gitRevision()
        {
            return GNSSHAT_GIT_REVISION;
        }

        int buildNumber()
        {
            return GNSSHAT_BUILD_NUMBER;
        }
    }
}
