/*
 * Jimmy Paputto 2025
 */

#include <cstdio>
#include "GnssHat.hpp"
#include "common/BuildInfo.hpp"
#include "ntrip/NtripClient.hpp"

int main()
{
    printf("gnsshat %s (build %d, %s)\n",
           GNSS_HAT_VERSION,
           JimmyPaputto::BuildInfo::buildNumber(),
           JimmyPaputto::BuildInfo::gitRevision().data());

    printf("Built: %s %s\n",
           JimmyPaputto::BuildInfo::buildDate().data(),
           JimmyPaputto::BuildInfo::buildTime().data());

#if defined(RPI5)
    printf("Platform: Raspberry Pi 5\n");
#elif defined(RPI4)
    printf("Platform: Raspberry Pi 4\n");
#else
    printf("Platform: unknown\n");
#endif

    printf("\nFeatures:\n");
    printf("  NTRIP TLS support: %s\n",
           JimmyPaputto::NtripClient::isTlsAvailable() ? "enabled" : "disabled");

    return 0;
}
