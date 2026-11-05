# usb-host-msc

[![arduino-library-badge](https://img.shields.io/badge/Arduino-Library-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/reference/en/libraries/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Plug an ordinary USB flash drive into your board's USB **host** port and
read/write files on it -- no SD card or card slot involved. A small
Arduino wrapper around TinyUSB's host-mode Mass Storage driver (the
TinyUSB built into the ESP32 and RP2040 cores, or the
[Adafruit TinyUSB Library](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)
elsewhere):

- `USBHostMSC` -- blocking SCSI block access (`readBlocks()`, `writeBlocks()`, `capacity()`, `inquiry()`)
- `USBMscFatFS` -- FAT12/16/32 and exFAT via [TinyFATFS](https://github.com/pschatzmann/TinyFATFS)
- `USBMscExt4FS` / `USBMscExt4FSReadOnly` -- ext2/3/4 via [TinyEXT4](https://github.com/pschatzmann/TinyEXT4)
- on ESP32/RP2040 the mounted drive is a standard Arduino `fs::FS`, so SD.h/LittleFS.h-style code works unchanged

Targets: ESP32-S2/S3, RP2040, STM32F4/F7, ESP32-P4, and any board via an external MAX3421E chip.

## Installation

Install this library from the Library Manager; *TinyFATFS* and *TinyEXT4*
come with it. Then select USB host mode for your board -- see
[Supported targets and board configuration](docs/BOARDS.md):

- **ESP32-S2/S3** (arduino-esp32 >= 3.3): USB Mode "USB-OTG (TinyUSB)".
  Nothing else to install.
- **RP2040**: USB Stack "Adafruit TinyUSB Host (native)". Nothing else to
  install.
- **STM32, ESP32-P4, boards with a MAX3421E chip**: also install the
  *Adafruit TinyUSB Library* (plus the small patch described in the board
  docs).

## Quick start

```cpp
#include "USBHostMSC.h"
#include "USBMscFatFS.h"

USBMscFatFS fatFs;
USBHostMSC msc(&fatFs);   // auto-mounts fatFs when a drive is plugged in

void onMount(uint8_t dev_addr) {
  if (fatFs.mounted()) {
    File f = fatFs.fs().open("/hello.txt", "r");
    while (f.available()) Serial.write(f.read());   // Serial1 on RP2040/STM32
  }
}

void setup() {
  Serial.begin(115200);   // Serial1 on RP2040/STM32
  msc.onMount(onMount);
  msc.begin();
}

void loop() { msc.task(); }
```

## Documentation

- [Supported targets and board configuration](docs/BOARDS.md)
  - [ESP32-S2/S3/P4 native host](docs/ESP32_NATIVE.md)
  - [STM32](docs/STM32.md)
  - [MAX3421E (boards without native USB)](docs/MAX3421E.md)
- [Wiring: one USB port for drive, power and Serial](docs/SINGLE_PORT.md)
- [Block-device API (`USBHostMSC`)](docs/BLOCK_API.md)
- [Filesystems: FAT, exFAT, ext2/3/4, auto-detection](docs/FILESYSTEMS.md)
- [Hubs and multiple devices](docs/MULTIPLE_DEVICES.md)
- [Examples](docs/EXAMPLES.md)
- [Testing (desktop CMake tests, firmware compile tests)](docs/TESTING.md)
- [Licensing and dependencies](docs/LICENSING.md)

## License

MIT. See [docs/LICENSING.md](docs/LICENSING.md) for the dependencies' licenses.
