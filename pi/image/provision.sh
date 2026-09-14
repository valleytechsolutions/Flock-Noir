#!/usr/bin/env bash
# =============================================================================
#  Flock Noir - image provisioning
#  Runs INSIDE the Raspberry Pi OS image (chroot under QEMU) during the GitHub
#  Actions build. Turns a stock Raspberry Pi OS Lite card into a "flash and go"
#  Flock Noir appliance:
#    - Flock Noir installed as a systemd service (starts on every boot)
#    - the "Flock Noir" Wi-Fi hotspot pre-configured (boots straight into it)
#    - SSH on, SPI (IR receivers) and UART (GPS) enabled, serial console off
#    - default login  user: flock  password: flocknoir   (change it!)
# =============================================================================
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
REPO=/opt/flocknoir
BOOT=/boot/firmware; [ -f "$BOOT/config.txt" ] || BOOT=/boot

echo "== Flock Noir image provisioning =="

# 1) same installer people run by hand; it skips the service restart when
#    systemd is not running (we are in a chroot)
bash "$REPO/pi/install.sh"

# 2) hostname
echo flocknoir > /etc/hostname
sed -i 's/^127\.0\.1\.1.*/127.0.1.1\tflocknoir/' /etc/hosts || true
grep -q '127.0.1.1' /etc/hosts || echo -e '127.0.1.1\tflocknoir' >> /etc/hosts

# 3) default login, applied by Raspberry Pi OS on first boot
HASH="$(openssl passwd -6 'flocknoir')"
echo "flock:${HASH}" > "$BOOT/userconf.txt"

# 4) SSH on
touch "$BOOT/ssh"
systemctl enable ssh >/dev/null 2>&1 || true

# 5) SPI for the MCP3008 IR receivers, UART for a GPS, console off the serial port
grep -q '^dtparam=spi=on' "$BOOT/config.txt" || echo 'dtparam=spi=on' >> "$BOOT/config.txt"
grep -q '^enable_uart=1'  "$BOOT/config.txt" || echo 'enable_uart=1'  >> "$BOOT/config.txt"
sed -i 's/console=serial0,115200 //' "$BOOT/cmdline.txt" || true

# 6) field hotspot, pre-configured as a NetworkManager profile, plus captive-portal DNS
mkdir -p /etc/NetworkManager/system-connections /etc/NetworkManager/dnsmasq-shared.d
cat > /etc/NetworkManager/system-connections/FlockNoirAP.nmconnection <<'EOF'
[connection]
id=FlockNoirAP
type=wifi
interface-name=wlan0
autoconnect=true
autoconnect-priority=100

[wifi]
mode=ap
ssid=Flock Noir
band=bg

[wifi-security]
key-mgmt=wpa-psk
psk=flocknoir

[ipv4]
method=shared
address1=192.168.4.1/24

[ipv6]
method=disabled
EOF
chmod 600 /etc/NetworkManager/system-connections/FlockNoirAP.nmconnection
echo 'address=/#/192.168.4.1' > /etc/NetworkManager/dnsmasq-shared.d/flocknoir.conf

# 7) shrink
apt-get clean
rm -rf /var/lib/apt/lists/* /root/.cache/pip
echo "== provisioning complete =="
