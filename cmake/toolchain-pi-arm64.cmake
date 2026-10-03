# Cross-compiles for the Pi (64-bit Raspberry Pi OS) with Debian's aarch64
# cross-compiler, inside the image from tools/cross/Dockerfile. pigpio's
# cross-built copy lives in /opt/pigpio there.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Libraries and headers come from the target's side only; build tools from
# the machine doing the building.
set(CMAKE_FIND_ROOT_PATH /opt/pigpio)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
