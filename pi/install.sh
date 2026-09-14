#!/usr/bin/env bash
# =============================================================================
#  Flock Noir - Raspberry Pi installer
#  Tested target: Raspberry Pi Zero 2 W + Camera Module NoIR v2, Raspberry Pi OS
#  (Bookworm or newer, Lite is fine). Works on any Pi with Wi-Fi + a CSI camera.
#
#    sudo ./pi/install.sh          install deps, data dir, systemd service
#    sudo ./pi/netmode.sh hotspot  (afterwards) turn on the field Wi-Fi hotspot
# =============================================================================
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then echo "Run as root:  sudo $0"; exit 1; fi

PI_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$PI_DIR/.." && pwd)"
DATA_DIR="/var/lib/flocknoir"

echo "== Flock Noir installer =="
echo "   repo: $REPO_DIR"

echo "== apt packages =="
apt-get update
apt-get install -y --no-install-recommends \
  python3 python3-venv python3-pip \
  python3-picamera2 python3-numpy python3-pil python3-gpiozero python3-serial \
  iw ffmpeg git

echo "== python venv (system site packages for picamera2/gpiozero/numpy) =="
if [ ! -x "$PI_DIR/venv/bin/python" ]; then
  python3 -m venv --system-site-packages "$PI_DIR/venv"
fi
"$PI_DIR/venv/bin/pip" install --quiet --upgrade pip
"$PI_DIR/venv/bin/pip" install --quiet pynmea2

echo "== data directory =="
mkdir -p "$DATA_DIR"/{logs,wardrive,videos}

echo "== systemd service =="
sed "s|__PI__|$PI_DIR|g" "$PI_DIR/flocknoir.service" > /etc/systemd/system/flocknoir.service
if [ -d /run/systemd/system ]; then
  systemctl daemon-reload
  systemctl enable flocknoir >/dev/null
  systemctl restart flocknoir
else
  # image build / chroot: systemd is not running - enable only, it starts on first boot
  systemctl enable flocknoir >/dev/null 2>&1 || true
  echo "(no running systemd: service enabled, will start on boot)"
fi

echo
echo "Installed. The service is running:  systemctl status flocknoir"
echo "Logs:                                journalctl -u flocknoir -f"
echo
echo "Next: turn on the field hotspot (SSID 'Flock Noir', password 'flocknoir'):"
echo "      sudo $PI_DIR/netmode.sh hotspot"
echo "NOTE: a Pi Zero 2 W has one radio - the hotspot replaces your home Wi-Fi"
echo "      connection. SSH keeps working over the hotspot at 192.168.4.1."
