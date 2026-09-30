#include "hw/pigpio_session.hpp"

#include <pigpio.h>

#include <stdexcept>
#include <string>

namespace winampdeck {

PigpioSession::PigpioSession() {
    // All configuration has to happen before gpioInitialise().
    gpioCfgClock(5, PI_CLOCK_PWM, 0);
    gpioCfgInterfaces(PI_DISABLE_FIFO_IF | PI_DISABLE_SOCK_IF);
    gpioCfgSetInternals(gpioCfgGetInternals() | PI_CFG_NOSIGHANDLER);

    if (const int status = gpioInitialise(); status < 0) {
        throw std::runtime_error(
            "pigpio failed to start (error " + std::to_string(status) +
            "). Run as root (sudo), and make sure the pigpiod daemon isn't running.");
    }
}

PigpioSession::~PigpioSession() {
    gpioTerminate();
}

}  // namespace winampdeck
