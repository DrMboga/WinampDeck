#pragma once

namespace winampdeck {

// Owns pigpio's process-wide initialisation. Exactly one may exist at a time,
// and it must outlive every object that makes pigpio calls.
//
// pigpio is configured for this Deck rather than left at its defaults:
// - It paces its DMA sampling with the PWM peripheral instead of PCM, because
//   PCM is the I2S block the Digi Pro HAT plays audio through.
// - Its socket and pipe interfaces are off: nothing outside this process needs
//   to drive the pins.
// - It installs no signal handlers, so Ctrl+C / SIGTERM reach the event loop
//   and the panel gets switched off cleanly.
class PigpioSession {
public:
    // Throws std::runtime_error if pigpio can't start (not root, or the pigpiod
    // daemon already owns the hardware).
    PigpioSession();
    ~PigpioSession();

    PigpioSession(const PigpioSession&) = delete;
    PigpioSession& operator=(const PigpioSession&) = delete;
};

}  // namespace winampdeck
