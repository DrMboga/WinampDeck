# Builds WinampDeck for the Pi on a PC (see tools/deploy-pi.sh and
# docs/pi-setup.md). The Pi 3 can't comfortably compile it itself: one file
# already ran it out of memory.
#
# Debian Trixie, to match the Pi's Raspberry Pi OS Trixie: the same GCC 14 and
# glibc 2.41, so what's built here runs there. It holds two compilers:
# - the native one, for the hardware-free unit tests, which run here;
# - the aarch64 cross-compiler, for the binaries that go to the Pi.
FROM debian:trixie

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential crossbuild-essential-arm64 cmake ninja-build make git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# pigpio isn't packaged for Trixie, so the Pi's copy was built from source
# (docs/pi-setup.md, 8.3). This cross-builds the same commit, only to compile
# and link against: at runtime the binaries use the Pi's own libpigpio.so.1.
ARG PIGPIO_COMMIT=c33738a320a3e28824af7807edafda440952c05d
RUN git clone https://github.com/joan2937/pigpio.git /tmp/pigpio \
    && cd /tmp/pigpio \
    && git checkout --quiet ${PIGPIO_COMMIT} \
    && make CROSS_PREFIX=aarch64-linux-gnu- libpigpio.so \
    && mkdir -p /opt/pigpio/include /opt/pigpio/lib \
    && cp pigpio.h /opt/pigpio/include/ \
    && cp -P libpigpio.so* /opt/pigpio/lib/ \
    && rm -rf /tmp/pigpio
