#pragma once

namespace winampdeck {

// The OS the Deck runs on.
class SystemControl {
public:
    virtual ~SystemControl() = default;

    // Issue a clean OS poweroff, after which it's safe to remove power.
    virtual void safeShutdown() = 0;
};

}  // namespace winampdeck
