/*
 * Jimmy Paputto 2026
 */

#ifndef GNSS_HAT_BUILD_INFO_HPP_
#define GNSS_HAT_BUILD_INFO_HPP_

#include <string_view>

namespace JimmyPaputto
{
    namespace BuildInfo
    {
        std::string_view buildDate();
        std::string_view buildTime();
        std::string_view gitRevision();
        int buildNumber();
    }
}

#endif // GNSS_HAT_BUILD_INFO_HPP_
