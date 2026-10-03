#!/usr/bin/env bash
# Builds WinampDeck for the Pi on this machine, in Docker, and copies the
# result to the Deck. Run from Git Bash, or any shell with docker, ssh and scp:
#
#   tools/deploy-pi.sh [--skip-tests] [--no-deploy] [USER@HOST]
#
#   --skip-tests   don't build and run the unit tests first
#   --no-deploy    build only; the binaries stay in build-pi/
#   USER@HOST      the Pi (default: pi@WinampDeck)
#
# On the Pi, everything lands in ~/winampdeck-deploy/:
#   winampdeck, winampdeck-panel-test   the binaries
#   data/                               config.json, stations.csv, logos/
#   VERSION                             the commit they were built from
# Nothing outside that directory is touched: sudo on the Pi asks for a
# password, so installing into /etc/winampdeck is a step to run there by hand
# (docs/pi-setup.md, Phase 8).
set -euo pipefail

pi=pi@WinampDeck
run_tests=1
deploy=1
for arg in "$@"; do
    case "$arg" in
    --skip-tests) run_tests=0 ;;
    --no-deploy) deploy=0 ;;
    -*) echo "unknown option $arg" >&2; exit 2 ;;
    *) pi=$arg ;;
    esac
done

cd "$(dirname "$0")/.."
# Docker on Windows wants C:/... paths, which Git Bash's pwd -W gives.
repo=$(pwd -W 2>/dev/null || pwd)
image=winampdeck-cross
out=build-pi

version=$(git rev-parse --short HEAD)
if [ -n "$(git status --porcelain)" ]; then
    version="$version+uncommitted"
fi

echo "== Build image"
docker build --quiet --tag "$image" tools/cross >/dev/null

# MSYS_NO_PATHCONV stops Git Bash rewriting the container-side paths.
in_container() {
    MSYS_NO_PATHCONV=1 docker run --rm \
        --volume "$repo:/src:ro" \
        --volume winampdeck-cross-build:/build \
        --volume "$repo/$out:/out" \
        "$image" bash -euo pipefail -c "$1"
}

mkdir -p "$out"

if [ "$run_tests" = 1 ]; then
    echo "== Unit tests (native)"
    in_container '
        cmake -S /src -B /build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_COMPILE_WARNING_AS_ERROR=ON >/dev/null
        cmake --build /build/native
        ctest --test-dir /build/native --output-on-failure | tail -3'
fi

echo "== Pi binaries (aarch64)"
in_container '
    cmake -S /src -B /build/pi -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-pi-arm64.cmake \
        -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
        -DWINAMPDECK_WITH_PIGPIO=ON -DWINAMPDECK_BUILD_TESTS=OFF >/dev/null
    cmake --build /build/pi --target winampdeck winampdeck-panel-test
    cp /build/pi/src/winampdeck /build/pi/src/winampdeck-panel-test /out/'
echo "$version" > "$out/VERSION"
echo "Built $version into $out/"

if [ "$deploy" = 1 ]; then
    echo "== Deploy to $pi"
    # A running binary can't be overwritten, but it can be replaced: upload
    # next to it, then rename over it. The running controller carries on with
    # the old one until it's restarted.
    ssh "$pi" 'rm -rf ~/winampdeck-deploy/.incoming && mkdir -p ~/winampdeck-deploy/.incoming'
    scp -q "$out/winampdeck" "$out/winampdeck-panel-test" "$out/VERSION" "$pi:winampdeck-deploy/.incoming/"
    scp -q -r data "$pi:winampdeck-deploy/"
    ssh "$pi" 'cd ~/winampdeck-deploy && chmod +x .incoming/winampdeck .incoming/winampdeck-panel-test \
        && mv -f .incoming/* . && rmdir .incoming \
        && echo "On the Pi: ~/winampdeck-deploy, version $(cat VERSION)" && ls -l winampdeck winampdeck-panel-test'
fi
