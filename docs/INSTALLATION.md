# Installation

Install this library from the Library Manager; *TinyFATFS* and *TinyEXT4*
come with it. Then select the USB mode for your board -- host and device
mode need different settings, see
[Supported targets and board configuration](BOARDS.md).

| Board | Host mode (read a stick) | Device mode (be a stick) |
|---|---|---|
| **ESP32-S2/S3** (arduino-esp32 >= 3.3) | USB Mode "USB-OTG (TinyUSB)" | USB Mode "USB-OTG (TinyUSB)" (same setting) |
| **RP2040** | USB Stack "Adafruit TinyUSB Host (native)" | USB Stack "Adafruit TinyUSB" |
| **STM32, ESP32-P4, MAX3421E boards** | also install the *Adafruit TinyUSB Library* (plus the small patch described in the board docs) | -- (not covered) |
| **Other Adafruit TinyUSB boards** (SAMD, nRF52) | via MAX3421E chip | install the *Adafruit TinyUSB Library*; compiles, untested |

Nothing else is needed on ESP32-S2/S3 and RP2040. The SD card examples of
device mode additionally need *SdFat* (bundled with the RP2040 core).

