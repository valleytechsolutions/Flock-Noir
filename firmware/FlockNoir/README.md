# Flock Noir firmware - XIAO ESP32-S3 Sense

Use the pinned Arduino framework through the root [PlatformIO configuration](../../platformio.ini).
The tested target has 8 MB flash, OPI PSRAM, ATGM336H UART at 9600 baud,
OPT101 OUT on D1/GPIO2, OV2640 on the Sense ribbon and microSD CS on GPIO21.

```bash
python tools/html2header.py
python -m platformio run -e xiao_esp32s3_sense
python -m platformio run -e xiao_esp32s3_sense -t upload --upload-port COM44
```

Run these from the repository root; replace the port. See the
[installation guide](../../README.md#install-and-flash-xiao-esp32-s3-sense),
[wiring](../../HARDWARE.md), [radio feature guide](../../docs/RADIO.md) and
[credits](../../ATTRIBUTIONS.md). No arbitrary ESP32 core versions: this radio
implementation uses the NimBLE host supplied by the pinned platform.

`config.h` owns pins and defaults. `pulse_detector.h` and `radio_protocol.h`
contain host-testable detection/parsing. `irsensor.cpp` samples independently;
`radio.cpp` queues and processes observations outside driver callbacks.
`radio_routes.ino` exposes radio APIs through the existing server.
`web_ui.h` is generated from `web/index.html`; edit the HTML and regenerate.

Host checks (C++17 compiler required):

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I firmware/FlockNoir tests/detection_test.cpp firmware/FlockNoir/detector.cpp -o detection_test
./detection_test
```

Software tests do not replace the optical, radio-coexistence and GPS/SD hardware
checks in the radio guide. An alert indicates evidence, not confirmed identity.
