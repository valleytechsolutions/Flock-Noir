# Ready-to-flash Raspberry Pi image

The GitHub Actions workflow `.github/workflows/build-pi-image.yml` produces
`flocknoir-pi.img.xz`: the official Raspberry Pi OS Lite (64-bit) with Flock Noir
pre-installed and pre-configured. Flash it with Raspberry Pi Imager ("Use custom"),
boot, and join the **Flock Noir** Wi-Fi. No setup steps.

## What the image contains

- Flock Noir installed at `/opt/flocknoir`, running as the `flocknoir` systemd service.
- The **Flock Noir** hotspot (password `flocknoir`) pre-configured to start on boot at
  `192.168.4.1`, with a captive portal so the page opens automatically.
- SSH enabled. Default login **flock / flocknoir**. Change it after first boot:
  `passwd`.
- SPI enabled (MCP3008 IR receivers) and UART enabled with the serial console off (GPS).
- Hostname `flocknoir`.

## Getting the image

- **Releases:** every tagged release (`v*`) has `flocknoir-pi.img.xz` and a `.sha256`
  attached.
- **Any time:** Actions -> "Build Raspberry Pi image" -> Run workflow -> download the
  artifact. (Building takes roughly 30-60 minutes under emulation.)

## How it is built

`pguyot/arm-runner-action` downloads the base image, grows it, mounts it, copies this
repo to `/opt/flocknoir`, and runs `pi/image/provision.sh` inside it with QEMU. That
script calls the normal `pi/install.sh`, then applies the appliance settings above,
cleans apt/pip caches, and the action shrinks the image back down before it is
xz-compressed and published.

## Getting the Pi onto your home Wi-Fi (for updates)

The Pi has one radio, so the hotspot and your home network are exclusive. SSH in over
the hotspot and switch:

```bash
ssh flock@192.168.4.1
sudo /opt/flocknoir/pi/netmode.sh client "Your Home SSID" "your-password"
```
Then `cd /opt/flocknoir && git pull && sudo ./pi/install.sh` to update, and
`sudo /opt/flocknoir/pi/netmode.sh hotspot` to go back to field mode.
