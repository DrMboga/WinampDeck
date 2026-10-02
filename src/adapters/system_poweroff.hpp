#pragma once

#include "core/system_control.hpp"

namespace winampdeck {

// Safe Shutdown through systemd: `systemctl poweroff`, which stops every
// service (this controller included, by SIGTERM) and then powers off. Needs
// root, which the controller has anyway for pigpio.
class SystemPoweroff final : public SystemControl {
public:
    void safeShutdown() override;
};

}  // namespace winampdeck
