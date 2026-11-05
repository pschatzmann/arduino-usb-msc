# usb-msc

[![arduino-library-badge](https://img.shields.io/badge/Arduino-Library-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/reference/en/libraries/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

USB **Mass Storage** for Arduino -- the "USB stick" protocol, in both directions:

- **USB host:** your board is the computer. Plug a USB flash drive into the
  board and read/write its files (FAT, exFAT, ext2/3/4), no SD card slot needed.
- **USB device:** your board is the USB stick. Plug it into a PC and it shows
  up as a drive, backed by RAM, an SD card or any storage you provide.

A board's USB port does one role at a time; pick it in the board's USB menu.

## Documentation

- [Installation and board settings](docs/INSTALLATION.md) -- [supported boards](docs/BOARDS.md)
- [Host vs. device overview](docs/OVERVIEW.md)
- Host mode: [quick start](docs/HOST_QUICKSTART.md), [block API](docs/BLOCK_API.md), [filesystems](docs/FILESYSTEMS.md), [hubs](docs/MULTIPLE_DEVICES.md), [ESP32](docs/ESP32_NATIVE.md), [STM32](docs/STM32.md), [MAX3421E](docs/MAX3421E.md), [single-port wiring](docs/SINGLE_PORT.md)
- Device mode: [quick start](docs/DEVICE_QUICKSTART.md), [API](docs/DEVICE_API.md)
- [Examples](docs/EXAMPLES.md), [Testing](docs/TESTING.md), [Licensing](docs/LICENSING.md)

## License

MIT. See [docs/LICENSING.md](docs/LICENSING.md).
