# USBEncoder

PlatformIO firmware for an ESP32-S3 DevKitC-1 and an AS5600 magnetic angle sensor.

## Wiring

| Signal | ESP32-S3 GPIO |
| --- | ---: |
| AS5600 SDA | 17 |
| AS5600 SCL | 18 |
| AS5600 DIR | 10 (output, LOW) |
| Digital input bit 0 | 4 (`INPUT_PULLUP`) |
| Digital input bit 1 | 5 (`INPUT_PULLUP`) |

The AS5600 uses its default I²C address, `0x36`. GPIO 4 is the least significant input bit, and GPIO 5 is the most significant. With the internal pull-ups, an open input reads HIGH; connect an input to ground to read LOW.

## Build and run

Open this folder in PlatformIO, build and upload the `esp32-s3-devkitc-1` environment, then open the serial monitor at 115200 baud. USB CDC is enabled for the ESP32-S3's native USB connection.

The firmware reports the angle relative to its saved zero position in the range −180° to +180°, along with the two input states packed as an integer from 0 to 3. It reports when the angle changes by at least 0.3°, when an input changes, and at least once every five seconds. Values strictly between −0.08° and +0.02° are reported as zero.

Send `zero` followed by Enter in the serial monitor to save the current sensor angle as zero. The offset is stored in ESP32 NVRAM and persists across restarts.

The repository's `LICENSE` applies to this project.
