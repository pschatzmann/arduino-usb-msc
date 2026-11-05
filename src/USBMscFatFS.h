#pragma once

// fs::FS/fs::File come from the core; only ESP32's and RP2040's Arduino
// cores ship one. STM32 gets its own implementation further below,
// returning USBMscFile instead of fs::File (see USBMscFile.h).
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)

#include <FS.h>

#include "USBHostMSC.h"
#include "USBMscBlockDevice.h"
#include "USBMscFSBackend.h"
#include "USBMscPartition.h"

namespace usbmscfs {
class FatBackend; // fs/USBMscFatBackend.h
}

/**
 * @brief Mounts a FAT12/16/32 or exFAT partition (or whole, unpartitioned disk) as
 * a standard Arduino fs::FS, using TinyFATFS.
 *
 * Referencing this class (as opposed to USBMscExt4FS) is what pulls the
 * FAT backend into your firmware -- there is no separate build flag.
 */
class USBMscFatFS : public USBMscFSBackend {
public:
  USBMscFatFS();
  ~USBMscFatFS() override;

  USBMscFatFS(const USBMscFatFS &) = delete;
  USBMscFatFS &operator=(const USBMscFatFS &) = delete;

  /// Picks the first FAT/exFAT-type MBR partition (or the whole disk, if
  /// unpartitioned or no FAT partition is found) and mounts it. Returns
  /// false if the boot sector doesn't look like FAT.
  bool begin(USBHostMSC &msc, uint8_t dev_addr) override;

  /// Same, for any USBMscBlockDevice (RAM disk, SD card, NBD export, ...)
  /// instead of a USB stick. The device must use 512-byte blocks and must
  /// stay valid until end() is called or this object is destroyed. Call
  /// end() (not end(dev_addr)) to unmount.
  bool begin(USBMscBlockDevice &dev);

  /// Convenience overload for direct callers that don't track dev_addr
  /// themselves -- this class only ever holds one mounted filesystem, so
  /// there's nothing to disambiguate.
  void end() { reset(); }
  void end(uint8_t dev_addr) override { (void)dev_addr; reset(); }

  bool mounted() const { return _fs != nullptr; }

  /// The mounted filesystem. Only valid after begin() returns true.
  fs::FS &fs() { return *_fs; }
  operator fs::FS &() { return *_fs; }

private:
  void reset();
  bool mountBound();

  USBMscPartition _partition;
  usbmscfs::FatBackend *_backend = nullptr;
  fs::FS *_fs = nullptr;
};

#elif defined(ARDUINO_ARCH_STM32)

#include "USBHostMSC.h"
#include "USBMscBlockDevice.h"
#include "USBMscFSBackend.h"
#include "USBMscFile.h"
#include "USBMscPartition.h"

namespace usbmscfs {
class Backend; // fs/USBMscFsBackend.h
}

/**
 * @brief Mounts a FAT12/16/32 or exFAT partition (or whole, unpartitioned disk),
 * using the same TinyFATFS backend as ESP32/RP2040 (see
 * fs/USBMscFatBackend.h) -- STM32duino just has no `fs::FS` to adapt it
 * to, so files are USBMscFile, not fs::File, and this class exposes
 * open()/exists()/etc. directly instead of through a fs::FS.
 *
 * Referencing this class is what pulls the FAT backend into your
 * firmware -- there is no separate build flag.
 */
class USBMscFatFS : public USBMscFSBackend {
public:
  USBMscFatFS();
  ~USBMscFatFS() override;

  USBMscFatFS(const USBMscFatFS &) = delete;
  USBMscFatFS &operator=(const USBMscFatFS &) = delete;

  /// Picks the first FAT/exFAT-type MBR partition (or the whole disk, if
  /// unpartitioned or no FAT partition is found) and mounts it. Returns
  /// false if the boot sector doesn't look like FAT.
  bool begin(USBHostMSC &msc, uint8_t dev_addr) override;

  /// Same, for any USBMscBlockDevice (RAM disk, SD card, NBD export, ...)
  /// instead of a USB stick. The device must use 512-byte blocks and must
  /// stay valid until end() is called or this object is destroyed. Call
  /// end() (not end(dev_addr)) to unmount.
  bool begin(USBMscBlockDevice &dev);

  /// Convenience overload for direct callers that don't track dev_addr
  /// themselves -- this class only ever holds one mounted filesystem, so
  /// there's nothing to disambiguate.
  void end() { reset(); }
  void end(uint8_t dev_addr) override { (void)dev_addr; reset(); }

  bool mounted() const { return _ready; }

  /// Returns *this, so sketches can write `fatFs.fs().open(...)`
  /// uniformly across ESP32/RP2040 (where this returns a real fs::FS)
  /// and STM32 (which has no separate fs::FS-equivalent type to return).
  USBMscFatFS &fs() { return *this; }
  operator USBMscFatFS &() { return *this; }

  USBMscFile open(const char *path, const char *mode = "r");
  bool exists(const char *path);
  bool remove(const char *path);
  bool rename(const char *path_from, const char *path_to);
  bool mkdir(const char *path);
  bool rmdir(const char *path);

private:
  void reset();
  bool mountBound();

  USBMscPartition _partition;
  usbmscfs::Backend *_backend = nullptr;
  bool _ready = false;
};

#endif // ARDUINO_ARCH_STM32
