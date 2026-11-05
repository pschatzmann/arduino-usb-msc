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

- [Class documentation (Doxygen)](https://pschatzmann.github.io/arduino-usb-msc/annotated.html)
- [Installation and supported boards](https://github.com/pschatzmann/arduino-usb-msc/wiki/BOARDS)
- [Host vs. device overview](https://github.com/pschatzmann/arduino-usb-msc/wiki/OVERVIEW)
- Host mode: [quick start](https://github.com/pschatzmann/arduino-usb-msc/wiki/HOST_QUICKSTART), [block API](https://github.com/pschatzmann/arduino-usb-msc/wiki/BLOCK_API), [filesystems](https://github.com/pschatzmann/arduino-usb-msc/wiki/FILESYSTEMS), [hubs](https://github.com/pschatzmann/arduino-usb-msc/wiki/MULTIPLE_DEVICES), [ESP32](https://github.com/pschatzmann/arduino-usb-msc/wiki/ESP32_NATIVE), [STM32](https://github.com/pschatzmann/arduino-usb-msc/wiki/STM32), [MAX3421E](https://github.com/pschatzmann/arduino-usb-msc/wiki/MAX3421E), [single-port wiring](https://github.com/pschatzmann/arduino-usb-msc/wiki/SINGLE_PORT)
- Device mode: [quick start](https://github.com/pschatzmann/arduino-usb-msc/wiki/DEVICE_QUICKSTART), [API](https://github.com/pschatzmann/arduino-usb-msc/wiki/DEVICE_API)
- [Examples](https://github.com/pschatzmann/arduino-usb-msc/wiki/EXAMPLES), [Testing](https://github.com/pschatzmann/arduino-usb-msc/wiki/TESTING), [Licensing](https://github.com/pschatzmann/arduino-usb-msc/wiki/LICENSING)

## License

MIT. See [Licensing](https://github.com/pschatzmann/arduino-usb-msc/wiki/LICENSING).
