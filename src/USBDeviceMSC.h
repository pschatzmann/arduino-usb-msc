/*
 * usb-msc -- USB *device* mode Mass Storage
 *
 * The counterpart of USBHostMSC: instead of reading a USB stick, the board
 * *is* one. A PC that is plugged into the board's USB port sees a drive
 * whose sectors are served by a block driver of TinyFATFS or TinyEXT4
 * (RamIO, ArduinoSpiIO, Esp32SdmmcIO, NBDClientIO, ...) or by your own class
 * with the same disk_read()/disk_write()/disk_ioctl() interface.
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
 * @brief Exports a block driver to the USB host (PC) as a USB drive.
 *
 * The driver can be any class with the FatFs style block interface
 * (disk_initialize(), disk_status(), disk_read(), disk_write(),
 * disk_ioctl()): every fatfs::IO of TinyFATFS and every tinyext4::IO of
 * TinyEXT4, or your own class. Only 512-byte sectors are supported.
 *
 * @code
 * #include "USBDeviceMSC.h"
 * #include "fatfs.h"
 *
 * fatfs::RamIO ram(256);           // 128 KB, formatted by SDClass::begin()
 * USBDeviceMSC msc;
 * void setup() { msc.begin(ram); }
 * @endcode
 */
class USBDeviceMSC {
public:
  USBDeviceMSC();
  ~USBDeviceMSC();

  /// Optional, call before begin(). Max lengths 8 / 16 / 4 characters.
  void setID(const char *vendor, const char *product, const char *revision);

  /// Registers the MSC interface and exports `io` (which must outlive the
  /// export). Must be called before the USB stack starts: on boards where
  /// the core starts USB itself (RP2040, SAMD, nRF52) call it first thing
  /// in setup(), before Serial.begin(); on ESP32-S2/S3 it calls USB.begin()
  /// for you unless startUsb is false (set it false if you add other USB
  /// classes and call USB.begin() yourself).
  template <class IO> bool begin(IO &io, bool startUsb = true) {
    using Sig = decltype(sigOf(&IO::disk_read));
    using Cmd = decltype(cmdOf(&IO::disk_ioctl));
    if (io.disk_initialize(0) & (kNoInit | kNoDisk)) {
      return false;
    }
    typename Sig::Lba sectors = 0;
    if ((int)io.disk_ioctl(0, (Cmd)kGetSectorCount, &sectors) != 0 ||
        sectors == 0) {
      return false;
    }
    uint16_t size = 0; // GET_SECTOR_SIZE is optional (512 if unsupported)
    if ((int)io.disk_ioctl(0, (Cmd)kGetSectorSize, &size) == 0 && size != 0 &&
        size != kSectorSize) {
      return false;
    }
    Disk d;
    d.ctx = &io;
    d.read = [](void *ctx, uint32_t lba, void *buf, uint32_t n) {
      return (int)((IO *)ctx)->disk_read(0, (typename Sig::Buf)buf,
                                          (typename Sig::Lba)lba,
                                          (typename Sig::Count)n) == 0;
    };
    d.write = [](void *ctx, uint32_t lba, const void *buf, uint32_t n) {
      return (int)((IO *)ctx)->disk_write(0, (const uint8_t *)buf,
                                           (typename Sig::Lba)lba,
                                           (typename Sig::Count)n) == 0;
    };
    d.flush = [](void *ctx) {
      ((IO *)ctx)->disk_ioctl(0, (Cmd)kCtrlSync, nullptr);
    };
    d.sectors = (uint32_t)sectors;
    return _begin(d, startUsb);
  }

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
  static constexpr uint16_t kSectorSize = 512;
  // FatFs values, identical in TinyFATFS and TinyEXT4
  static constexpr int kNoInit = 0x01, kNoDisk = 0x02;
  static constexpr int kCtrlSync = 0, kGetSectorCount = 1, kGetSectorSize = 2;

  /// Type-erased driver
  struct Disk {
    void *ctx = nullptr;
    bool (*read)(void *, uint32_t, void *, uint32_t) = nullptr;
    bool (*write)(void *, uint32_t, const void *, uint32_t) = nullptr;
    void (*flush)(void *) = nullptr;
    uint32_t sectors = 0;
  };

  // parameter types of IO::disk_read(pdrv, buff, sector, count) and
  // IO::disk_ioctl(pdrv, cmd, buff), which differ per library
  template <class B, class L, class N> struct ReadSig {
    using Buf = B;
    using Lba = L;
    using Count = N;
  };
  template <class C, class R, class P, class B, class L, class N>
  static ReadSig<B, L, N> sigOf(R (C::*)(P, B, L, N));
  template <class C, class R, class P, class Cmd>
  static Cmd cmdOf(R (C::*)(P, Cmd, void *));

  bool _begin(const Disk &disk, bool startUsb);
  bool _readSectors(uint32_t lba, void *buf, uint32_t n) {
    return _disk.read(_disk.ctx, lba, buf, n);
  }
  bool _writeSectors(uint32_t lba, const void *buf, uint32_t n) {
    return _disk.write(_disk.ctx, lba, buf, n);
  }

  Disk _disk;
  bool _open = false;
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
