# Host vs. device: feature overview

| | **Host mode** -- the board reads a USB stick | **Device mode** -- the board *is* a USB stick |
|---|---|---|
| Plug into | a USB flash drive | a PC |
| Main class | `USBHostMSC` | `USBDeviceMSC` |
| Storage / filesystems | `USBMscFatFS`, `USBMscExt4FS`, `USBMscAutoFS`, `USBMscFSPool` (FAT/exFAT, ext2/3/4) | any `USBMscBlockDevice`: `USBMscRamDisk`, `USBMscSdCard`, or your own |
| Built on | host MSC driver of TinyUSB (`tuh_msc_*`) | the core's `USBMSC` (ESP32-S2/S3) or `Adafruit_USBD_MSC` |
| Boards | ESP32-S2/S3/P4, RP2040, STM32F4/F7, any board + MAX3421E | ESP32-S2/S3, RP2040 and other Adafruit TinyUSB boards |
| Examples | `USBHostMSC_*` | `USBDeviceMSC_*` |


Host and device mode use the USB controller in opposite roles, so one
controller cannot do both at once. On RP2040 you pick one with the *USB Stack*
menu; both together need two controllers (e.g. a MAX3421E host chip next to the
native device port). See [BOARDS.md](BOARDS.md).
