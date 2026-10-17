#!/usr/bin/env bash
# Installs what tools/deploy-pi.sh copied to the Pi, and (re)starts the Deck.
# Run on the Pi, from the deploy directory, after each deploy:
#
#   sudo ~/winampdeck-deploy/install.sh
#
# - binaries        -> /usr/local/bin
# - config.json, stations.csv, logos/ -> /etc/winampdeck, only where missing:
#                      files already there are yours and are left alone
# - systemd units   -> /etc/systemd/system, enabled so they start on boot
# - audio           -> the loopback module's files in /etc/modules-load.d and
#                      /etc/modprobe.d, and the module loaded; asound.conf ->
#                      /etc, only if there's none (an existing one is yours)
# Then it starts the Engines if they aren't running, restarts one only if its
# unit changed, and restarts the controller.
set -euo pipefail

if [ "$(id -u)" != 0 ]; then
    echo "Run it with sudo: sudo $0" >&2
    exit 1
fi
cd "$(dirname "$0")"

engines=(winampdeck-librespot winampdeck-mpv)
units=("${engines[@]}" winampdeck)

echo "== Binaries (version $(cat VERSION))"
install -m 755 winampdeck winampdeck-panel-test /usr/local/bin/

echo "== /etc/winampdeck"
mkdir -p /etc/winampdeck
cp --update=none data/config.json data/stations.csv /etc/winampdeck/
cp --update=none -r data/logos /etc/winampdeck/

echo "== Audio"
# The loopback card the spectrum analyzer records from, loaded at every boot
# and now. If it's already loaded, changed options only apply after a reboot.
install -m 644 audio/modules-load.d/snd-aloop.conf /etc/modules-load.d/
install -m 644 audio/modprobe.d/snd-aloop.conf /etc/modprobe.d/
modprobe snd-aloop || echo "WARNING: couldn't load snd-aloop; the spectrum will stay flat"
if [ ! -e /etc/asound.conf ]; then
    install -m 644 audio/asound.conf /etc/
fi

echo "== systemd units"
changed=()
# The Phase 12 experiment pointed mpv at plug:deck with a drop-in; the unit
# does that itself now.
experiment=/etc/systemd/system/winampdeck-mpv.service.d/phase12-tap.conf
if [ -e "$experiment" ]; then
    rm "$experiment"
    rmdir --ignore-fail-on-non-empty "$(dirname "$experiment")"
    changed+=(winampdeck-mpv)
fi
for unit in "${units[@]}"; do
    if ! cmp -s "systemd/$unit.service" "/etc/systemd/system/$unit.service"; then
        install -m 644 "systemd/$unit.service" /etc/systemd/system/
        changed+=("$unit")
    fi
done
systemctl daemon-reload
systemctl enable --quiet "${units[@]}"

# What the units expect to find (docs/pi-setup.md, Phases 2, 3 and 5).
[ -x /home/pi/go-librespot/go-librespot ] || echo "WARNING: /home/pi/go-librespot/go-librespot is missing"
[ -x /usr/bin/mpv ] || echo "WARNING: /usr/bin/mpv is missing (sudo apt install mpv)"
# Both Engines play to plug:deck, which only an up-to-date asound.conf defines.
grep -q '^pcm\.deck ' /etc/asound.conf ||
    echo "WARNING: /etc/asound.conf has no \"deck\" device; compare it with $PWD/audio/asound.conf"
grep -q '^audio_device: plug:deck$' /home/pi/go-librespot/config.yml 2>/dev/null ||
    echo "WARNING: go-librespot isn't set to audio_device: plug:deck in /home/pi/go-librespot/config.yml"
[ -e /proc/asound/Loopback ] || echo "WARNING: the loopback card isn't there (cat /proc/asound/cards)"
if systemctl is-active --quiet pigpiod 2>/dev/null; then
    echo "WARNING: pigpiod is running; the controller needs the GPIO to itself (sudo systemctl disable --now pigpiod)"
fi
# Engines started by hand would hold the audio device and go-librespot's port.
if pgrep -x mpv >/dev/null && ! systemctl is-active --quiet winampdeck-mpv; then
    echo "WARNING: an mpv started by hand is running; stop it (tmux kill-session -t mpv)"
fi
if pgrep -x go-librespot >/dev/null && ! systemctl is-active --quiet winampdeck-librespot; then
    echo "WARNING: a go-librespot started by hand is running; stop it (tmux kill-session -t librespot)"
fi

echo "== Start"
for engine in "${engines[@]}"; do
    if [[ " ${changed[*]} " == *" $engine "* ]]; then
        systemctl restart "$engine"
    else
        systemctl start "$engine"
    fi
done
systemctl reset-failed winampdeck 2>/dev/null || true
systemctl restart winampdeck

sleep 2
systemctl --no-pager --lines=0 status "${units[@]}" | grep -E "●|Active:" || true
echo "Logs: journalctl -u winampdeck -u winampdeck-librespot -u winampdeck-mpv -f"
