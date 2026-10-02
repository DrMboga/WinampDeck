#include "adapters/system_poweroff.hpp"

#include <cstdlib>
#include <iostream>

namespace winampdeck {

void SystemPoweroff::safeShutdown() {
    std::cout << "Safe Shutdown: systemctl poweroff" << std::endl;
    // Returns as soon as systemd has queued the job; the SIGTERM comes after.
    if (const int status = std::system("systemctl poweroff"); status != 0) {
        std::cerr << "Safe Shutdown failed: systemctl poweroff returned " << status << std::endl;
    }
}

}  // namespace winampdeck
