"""Linux packet transports. Passive capture only; bounded input and retries.

WiFi uses a separately configured monitor interface; BLE uses legacy HCI
advertising reports. These functions do not change the dashboard interface.
"""
from pathlib import Path
import re
import socket
import struct
import subprocess
import time


def command(args):
    result = subprocess.run(args, capture_output=True, text=True, timeout=5)
    if result.returncode:
        raise OSError((result.stderr or result.stdout).strip()[:240] or "Command failed")
    return result.stdout


def prepare_monitor(interface, ap_interface, scan_interface):
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,15}", interface):
        raise ValueError("Set RADIO_MONITOR_IFACE to a dedicated USB WiFi interface")
    if interface in (ap_interface, scan_interface):
        raise ValueError("Monitor interface must differ from hotspot and survey interfaces")
    base = Path("/sys/class/net")
    phy = (base / interface / "phy80211").resolve(strict=True)
    ap_phy = base / ap_interface / "phy80211"
    if ap_phy.exists() and phy == ap_phy.resolve():
        raise ValueError("Monitor and hotspot interfaces must use different physical radios")
    info = command(["iw", "dev", interface, "info"])
    if not re.search(r"\btype monitor\b", info):
        # Only the explicitly configured dedicated adapter is changed.
        command(["nmcli", "device", "set", interface, "managed", "no"])
        command(["ip", "link", "set", "dev", interface, "down"])
        command(["iw", "dev", interface, "set", "type", "monitor"])
    command(["ip", "link", "set", "dev", interface, "up"])


def radiotap(packet):
    """Return raw 802.11, signal and channel; reject truncation/bad FCS."""
    if len(packet) < 8 or packet[0] != 0:
        return None
    length = struct.unpack_from("<H", packet, 2)[0]
    if not 8 <= length <= len(packet):
        return None
    present = word = struct.unpack_from("<I", packet, 4)[0]
    offset = 8
    while word & 0x80000000:
        if offset + 4 > length or offset > 32:
            return None
        word = struct.unpack_from("<I", packet, offset)[0]
        offset += 4
    flags, channel, rssi = 0, 0, -127
    # The first six standard fields are enough for FCS, channel and RSSI.
    for bit, alignment, size in ((0, 8, 8), (1, 1, 1), (2, 1, 1),
                                 (3, 2, 4), (4, 2, 2), (5, 1, 1)):
        if not present & (1 << bit):
            continue
        offset = (offset + alignment - 1) // alignment * alignment
        if offset + size > length:
            return None
        if bit == 1:
            flags = packet[offset]
        elif bit == 3:
            frequency = struct.unpack_from("<H", packet, offset)[0]
            channel = 14 if frequency == 2484 else (frequency-2407)//5 if 2412 <= frequency <= 2472 else 0
        elif bit == 5:
            rssi = struct.unpack_from("b", packet, offset)[0]
        offset += size
    if flags & 0x40:
        return None
    payload = packet[length:]
    if flags & 0x10:
        if len(payload) < 4:
            return None
        payload = payload[:-4]
    return payload, rssi, channel


def advertising_reports(packet):
    """Decode all reports in one legacy LE Meta event, rejecting bad bounds."""
    if len(packet) < 5 or packet[:2] != b"\x04\x3e" or packet[2]+3 != len(packet) or packet[3] != 2:
        return []
    offset, result = 5, []
    for _ in range(packet[4]):
        if offset+9 > len(packet):
            return []
        event, addr_type = packet[offset:offset+2]
        address = packet[offset+2:offset+8][::-1]
        length = packet[offset+8]
        offset += 9
        if length > 31 or offset+length+1 > len(packet):
            return []
        data = packet[offset:offset+length]
        rssi = struct.unpack_from("b", packet, offset+length)[0]
        result.append(dict(data=data, address=address, address_type=addr_type,
                           event_type=event, rssi=rssi))
        offset += length+1
    return result if offset == len(packet) else []


def hci_command(sock, opcode, parameters):
    sock.sendall(b"\x01" + struct.pack("<HB", opcode, len(parameters)) + parameters)
    deadline = time.monotonic()+2
    while time.monotonic() < deadline:
        try:
            packet = sock.recv(260)
        except socket.timeout:
            continue
        status = None
        if len(packet) >= 7 and packet[:2] == b"\x04\x0e" and struct.unpack_from("<H", packet, 4)[0] == opcode:
            status = packet[6]
        elif len(packet) >= 7 and packet[:2] == b"\x04\x0f" and struct.unpack_from("<H", packet, 5)[0] == opcode:
            status = packet[3]
        if status is not None:
            if status:
                raise OSError("BLE controller rejected command %04x (status %02x)" % (opcode, status))
            return
    raise TimeoutError("BLE controller command timed out")


def open_hci(index):
    command(["btmgmt", "--index", str(index), "power", "on"])
    sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_RAW, socket.BTPROTO_HCI)
    try:
        sock.bind((index,))
        sock.settimeout(0.25)
        sock.setsockopt(socket.SOL_HCI, 2, struct.pack("<IIIH2x", 1 << 4, 0xffffffff, 0xffffffff, 0))
        hci_command(sock, 0x200B, struct.pack("<BHHBB", 0, 800, 80, 0, 0))
        hci_command(sock, 0x200C, b"\x01\x00")
        return sock
    except Exception:
        sock.close()
        raise
