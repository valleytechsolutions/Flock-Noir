#!/usr/bin/env bash
# =============================================================================
#  Flock Noir - Raspberry Pi network mode (NetworkManager / nmcli)
#
#    sudo ./netmode.sh hotspot   field mode: Pi hosts Wi-Fi "Flock Noir" at 192.168.4.1
#    sudo ./netmode.sh client [SSID] [password]   home mode: join your Wi-Fi (for updates)
#
#  A Pi Zero 2 W has ONE radio, so hotspot and home Wi-Fi are exclusive.
#  SSH still works over the hotspot:  ssh <user>@192.168.4.1
# =============================================================================
set -euo pipefail
[ "$(id -u)" -eq 0 ] || { echo "Run as root: sudo $0 $*"; exit 1; }

SSID="Flock Noir"
PASS="flocknoir"          # 8+ chars (WPA2 minimum)
IP="192.168.4.1/24"
IFACE="wlan0"
CON="FlockNoirAP"
DNSCONF="/etc/NetworkManager/dnsmasq-shared.d/flocknoir.conf"

case "${1:-}" in
  hotspot)
    nmcli con delete "$CON" >/dev/null 2>&1 || true
    nmcli con add type wifi ifname "$IFACE" con-name "$CON" autoconnect yes ssid "$SSID" \
      802-11-wireless.mode ap 802-11-wireless.band bg \
      ipv4.method shared ipv4.addresses "$IP" \
      wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$PASS" >/dev/null
    # Captive portal: answer every DNS name with our IP so phones open the UI
    mkdir -p "$(dirname "$DNSCONF")"
    echo "address=/#/${IP%/*}" > "$DNSCONF"
    nmcli con up "$CON" >/dev/null
    echo "Hotspot '$SSID' is up at ${IP%/*}  (password: $PASS)"
    echo "Connect a phone to it; the Flock Noir page opens automatically."
    ;;
  client)
    nmcli con down "$CON" >/dev/null 2>&1 || true
    nmcli con modify "$CON" autoconnect no >/dev/null 2>&1 || true
    rm -f "$DNSCONF"
    nmcli device wifi rescan >/dev/null 2>&1 || true
    if [ -n "${2:-}" ]; then
      sleep 2
      nmcli device wifi connect "$2" ${3:+password "$3"} ifname "$IFACE"
      echo "Hotspot off. Joined '$2'."
    else
      echo "Hotspot off. NetworkManager will reconnect to your saved home Wi-Fi."
      echo "(To join a network:  $0 client \"SSID\" \"password\")"
    fi
    ;;
  *)
    echo "usage: $0 hotspot | client [SSID] [password]"; exit 1 ;;
esac
