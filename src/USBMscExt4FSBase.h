#pragma once

// fs::FS/fs::File come from the core; only ESP32's and RP2040's Arduino
// cores ship one. STM32 gets its own implementation further below,
// returning USBMscFile instead of fs::File (see USBMscFile.h) -- the
// underlying usbmscfs::Ext4Backend (TinyEXT4) is fully portable already,
// so this is just a different top-level wrapper around the same backend.
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)

#include <FS.h>

#include "USBHostMSC.h"
#include "USBMscBlockDevice.h"
#include "USBMscFSBackend.h"
#include "USBMscPartition.h"

namespace usbmscfs {
class Ext4Backend; // fs/USBMscExt4Backend.h
}

/**
 * @brief Shared implementation behind USBMscExt4FS (read/write) and
 * USBMscExt4FSReadOnly -- construct one of those, not this directly.
 *
 * Referencing either subclass is what pulls the ext4 backend (TinyEXT4,
 * MIT) into your firmware. There is no separate build flag.
 */
class USBMscExt4FSBase : public USBMscFSBackend {
public:
  ~USBMscExt4FSBase() override;

  USBMscExt4FSBase(const USBMscExt4FSBase &) = delete;
  USBMscExt4FSBase &operator=(const USBMscExt4FSBase &) = delete;

  /// Picks the first Linux-type MBR partition (or the whole disk, if
  /// unpartitioned or no such partition is found) and mounts it. Returns
  /// false if no ext2/3/4 superblock is found there.
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

  /// True if the mounted filesystem is read-only: always for
  /// USBMscExt4FSReadOnly, and for USBMscExt4FS when the volume is
  /// ext2/ext3, needs fsck, or uses features TinyEXT4 can't write.
  bool readOnly() const;

  /// The mounted filesystem. Only valid after begin() returns true.
  fs::FS &fs() { return *_fs; }
  operator fs::FS &() { return *_fs; }

protected:
  explicit USBMscExt4FSBase(bool read_only) : _read_only(read_only) {}

private:
  void reset();
  bool mountBound();

  bool _read_only;
  USBMscPartition _partition;
  usbmscfs::Ext4Backend *_backend = nullptr;
  fs::FS *_fs = nullptr;
};

#elif defined(ARDUINO_ARCH_STM32)

#include "USBHostMSC.h"
#include "USBMscBlockDevice.h"
#include "USBMscFSBackend.h"
#include "USBMscFile.h"
#include "USBMscPartition.h"

namespace usbmscfs {
class Ext4Backend; // fs/USBMscExt4Backend.h
}

/**
 * @brief Shared implementation behind USBMscExt4FS (read/write) and
 * USBMscExt4FSReadOnly -- construct one of those, not this directly.
 *
 * STM32duino has no `fs::FS`, so this returns USBMscFile (not fs::File)
 * from open(), and fs() returns USBMscExt4FSBase& (itself) instead of a
 * separate fs::FS -- same pattern as USBMscFatFS's STM32 branch. The
 * underlying usbmscfs::Ext4Backend (TinyEXT4) is unchanged from
 * ESP32/RP2040.
 *
 * Referencing either subclass is what pulls the ext4 backend (TinyEXT4,
 * MIT) into your firmware. There is no separate build flag.
 */
class USBMscExt4FSBase : public USBMscFSBackend {
public:
  ~USBMscExt4FSBase() override;

  USBMscExt4FSBase(const USBMscExt4FSBase &) = delete;
  USBMscExt4FSBase &operator=(const USBMscExt4FSBase &) = delete;

  /// Picks the first Linux-type MBR partition (or the whole disk, if
  /// unpartitioned or no such partition is found) and mounts it. Returns
  /// false if no ext2/3/4 superblock is found there.
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

  /// True if the mounted filesystem is read-only: always for
  /// USBMscExt4FSReadOnly, and for USBMscExt4FS when the volume is
  /// ext2/ext3, needs fsck, or uses features TinyEXT4 can't write.
  bool readOnly() const;

  /// Returns *this, so sketches can write `extFs.fs().open(...)`
  /// uniformly across ESP32/RP2040 (where this returns a real fs::FS)
  /// and STM32 (which has no separate fs::FS-equivalent type to return).
  USBMscExt4FSBase &fs() { return *this; }
  operator USBMscExt4FSBase &() { return *this; }

  USBMscFile open(const char *path, const char *mode = "r");
  bool exists(const char *path);
  bool remove(const char *path);
  bool rename(const char *path_from, const char *path_to);
  bool mkdir(const char *path);
  bool rmdir(const char *path);

protected:
  explicit USBMscExt4FSBase(bool read_only) : _read_only(read_only) {}

private:
  void reset();
  bool mountBound();

  bool _read_only;
  USBMscPartition _partition;
  usbmscfs::Backend *_backend = nullptr;
  bool _ready = false;
};

#endif // ARDUINO_ARCH_STM32
