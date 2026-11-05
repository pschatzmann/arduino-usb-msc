#pragma once

// See USBMscFatFS.h / USBMscExt4FSBase.h -- fs::FS is only available on
// ESP32 and RP2040.
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)

#include "USBMscExt4FSReadOnly.h"
#include "USBMscFSBackend.h"
#include "USBMscFatFS.h"

/**
 * @brief Convenience combinator: tries USBMscFatFS, then ExtType.
 *
 * Referencing this class pulls in *both* backends -- if you only ever expect one filesystem type, use
 * USBMscFatFS or USBMscExt4FS(ReadOnly) directly instead to keep the
 * other one out of your firmware image.
 *
 * ExtType defaults to USBMscExt4FSReadOnly: auto-mounting whatever drive
 * happens to be plugged in shouldn't risk writing to it unless you say
 * so. Pass USBMscExt4FS explicitly for read/write ext4 auto-detection:
 *
 * ```cpp
 * USBMscAutoFS<> mscFs;                     // FAT (r/w) + ext4 (read-only)
 * USBMscAutoFS<USBMscExt4FS> mscFsRW;        // FAT (r/w) + ext4 (r/w)
 * ```
 *
 * ExtType must have the same shape as USBMscExt4FS/USBMscExt4FSReadOnly:
 * `bool begin(USBHostMSC&, uint8_t)`, `void end(uint8_t)`,
 * `bool mounted()`, `fs::FS& fs()`.
 */
template <typename ExtType = USBMscExt4FSReadOnly>
class USBMscAutoFS : public USBMscFSBackend {
public:
  bool begin(USBHostMSC &msc, uint8_t dev_addr) override {
    if (_fat.begin(msc, dev_addr)) {
      _active = &_fat;
      return true;
    }
    if (_ext4.begin(msc, dev_addr)) {
      _active = &_ext4;
      return true;
    }
    _active = nullptr;
    return false;
  }

  void end(uint8_t dev_addr) override {
    if (_active) {
      _active->end(dev_addr);
    }
    _active = nullptr;
  }

  bool isFat() const { return _active == &_fat; }
  bool isExt4() const { return _active == &_ext4; }

  /// The mounted filesystem. Only valid after begin() returns true.
  fs::FS &fs() { return isExt4() ? _ext4.fs() : _fat.fs(); }
  operator fs::FS &() { return fs(); }

private:
  USBMscFatFS _fat;
  ExtType _ext4;
  USBMscFSBackend *_active = nullptr;
};

#endif // ARDUINO_ARCH_ESP32 || ARDUINO_ARCH_RP2040
