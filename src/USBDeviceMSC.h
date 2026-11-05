/*
 * usb-msc -- USB *device* mode Mass Storage
 *
 * The counterpart of USBHostMSC: instead of reading a USB stick, the board
 * *is* one. A PC that is plugged into the board's USB port sees a drive
 * whose blocks are served by any USBMscBlockDevice (RAM disk, SD card,
 * SPI flash, ... or your own subclass).
 *
 * Built on the device-mode MSC class that is already part of the platform:
 *   - ESP32-S2/S3 (arduino-esp32 core): the core's USBMSC. Select
 *     Tools > USB Mode > "USB-OTG (TinyUSB)".
 *   - everything else (RP2040, SAMD, nRF52, STM32, ...): Adafruit_USBD_MSC
 *     from the "Adafruit TinyUSB Library". On RP2040 select
 *     Tools > USB Stack > "Adafruit TinyUSB" (the *device* stack, not
 *     "...Host (native)").
 *
 * Device and host mode need separate USB controllers; this class and
 * USBHostMSC can only run at the same time on boards that have two
 * (e.g. RP2040 with PIO-USB for the host side, or a MAX3421E host chip).
 *
 * Only one USBDeviceMSC instance (a single LUN) is supported.
 *
 * Do not let the MCU modify a filesystem on the exported medium while the
 * PC has it mounted -- the PC caches aggressively and both sides would
 * corrupt each other. Use USBDeviceMSC::mediaPresent(false) to "eject"
 * while the MCU accesses the data, then mediaPresent(true) again.
 */
#pragma once

#include <stdint.h>
#include <string.h>

#include "USBMscBlockDevice.h"

#if defined(ARDUINO_ARCH_ESP32) && !defined(USBDEVMSC_USE_ADAFRUIT_TINYUSB)
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED && __has_include(<USBMSC.h>)
#define USBDEVMSC_ESP32_CORE 1
#endif
#endif
#ifndef USBDEVMSC_ESP32_CORE
#define USBDEVMSC_ESP32_CORE 0
#endif

#if USBDEVMSC_ESP32_CORE
#include <Arduino.h>
#include <USB.h>
#include <USBMSC.h>
#else
#if !__has_include(<Adafruit_TinyUSB.h>)
#error \
    "USBDeviceMSC needs the 'Adafruit TinyUSB Library' on this board " \
    "(install it from the Library Manager). It is NOT needed on ESP32-S2/S3."
#endif
#include <Adafruit_TinyUSB.h>
#if !(CFG_TUD_ENABLED && CFG_TUD_MSC)
#error \
    "USBDeviceMSC requires USB device mode with CFG_TUD_MSC. On RP2040 " \
    "select Tools > USB Stack > \"Adafruit TinyUSB\"."
#endif
#endif

/**
 * @brief Simple RAM disk. Pass your own buffer (size = blocks * 512) or
 * let it malloc one. The content is lost on reset; it is *not* formatted --
 * format() writes a minimal FAT12/16 filesystem so that hosts accept it
 * (requires at least ~64 blocks, i.e. 32 KB).
 */
class USBMscRamDisk : public USBMscBlockDevice {
public:
  USBMscRamDisk(uint32_t blocks, uint8_t *storage = nullptr);
  ~USBMscRamDisk();
  bool begin(bool formatFat = true);
  uint32_t blockCount() override { return _blocks; }
  bool readBlocks(uint32_t lba, void *buffer, uint32_t count) override;
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t count) override;
  /// Direct access to the backing store (e.g. for preloading).
  uint8_t *data() { return _data; }
  /// Writes an empty FAT12/FAT16 volume. Returns false if too small.
  bool format(const char *label = "RAMDISK");
  /// Adds a file to the root directory of the volume written by format().
  /// `name` is 8.3 ("HELLO.TXT", upper case). Files are stored contiguously,
  /// so call it right after format() and before the PC gets the drive.
  /// Returns false if the name is invalid or the volume/root is full.
  bool addFile(const char *name, const void *data, uint32_t len);

private:
  void setFat(uint32_t cluster, uint32_t value);
  uint32_t _blocks;
  // geometry of the volume written by format()
  uint8_t _spc = 0;
  bool _fat12 = false;
  uint32_t _fatSectors = 0, _rootStart = 0, _dataStart = 0, _clusters = 0;
  uint32_t _nextCluster = 2;
  uint16_t _rootUsed = 0, _rootEntries = 0;
  uint8_t *_data;
  bool _owned;
};

/**
 * @brief Exports a USBMscBlockDevice to the USB host (PC).
 *
 * @code
 * USBMscRamDisk disk(128);          // 64 KB
 * USBDeviceMSC msc;
 * void setup() { disk.begin(); msc.begin(disk); }
 * @endcode
 */
class USBDeviceMSC {
public:
  USBDeviceMSC();
  ~USBDeviceMSC();

  /// Optional, call before begin(). Max lengths 8 / 16 / 4 characters.
  void setID(const char *vendor, const char *product, const char *revision);

  /// Registers the MSC interface and exports `dev`. Must be called before
  /// the USB stack starts: on boards where the core starts USB itself
  /// (RP2040, SAMD, nRF52) call it first thing in setup(), before
  /// Serial.begin(); on ESP32-S2/S3 it calls USB.begin() for you unless
  /// startUsb is false (set it false if you add other USB classes and
  /// call USB.begin() yourself).
  bool begin(USBMscBlockDevice &dev, bool startUsb = true);

  void end();

  /// Reject (false) or allow (true) PC writes. Default: writable.
  void setWritable(bool writable) { _writable = writable; }
  bool writable() const { return _writable; }

  /// Tell the PC that the medium is inserted (true) or ejected (false).
  void mediaPresent(bool present);

  /// True once the PC has sent at least one read/write to the drive.
  bool hostActive() const { return _active; }
  /// True if the PC has written since the last clearWritten().
  bool written() const { return _written; }
  void clearWritten() { _written = false; }

  /// Invoked (from the USB task) after the PC wrote blocks. Keep it short.
  void onWrite(void (*cb)(uint32_t lba, uint32_t count)) { _onWrite = cb; }

  // --- internal, used by the static TinyUSB callbacks ---
  int32_t _read(uint32_t lba, uint32_t offset, void *buf, uint32_t size);
  int32_t _write(uint32_t lba, uint32_t offset, const uint8_t *buf,
                 uint32_t size);
  void _flush();

private:
  USBMscBlockDevice *_dev = nullptr;
  bool _writable = true;
  volatile bool _active = false;
  volatile bool _written = false;
  void (*_onWrite)(uint32_t, uint32_t) = nullptr;
  char _vendor[9] = "Arduino";
  char _product[17] = "USB Device MSC";
  char _rev[5] = "1.0";
#if USBDEVMSC_ESP32_CORE
  USBMSC _msc;
#else
  Adafruit_USBD_MSC _msc;
#endif
};
