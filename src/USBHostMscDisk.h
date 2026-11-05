#pragma once

#include <stdint.h>

// USBHOSTMSC_HOST_TEST: desktop unit tests swap the TinyUSB-backed
// USBHostMSC for an image-file stub with the same API (tests/stub).
#if defined(USBHOSTMSC_HOST_TEST)
#include <USBHostMSC_host_test.h>
#else
#include "USBHostMSC.h"
#endif

/**
 * @brief One USB mass storage device (LUN) as a device of 512-byte
 * sectors: the common part of the filesystem drivers USBHostMscFatIO
 * (TinyFATFS) and USBHostMscExt4IO (TinyEXT4).
 *
 * It is bound to a device by init() (the filesystem calls it when it
 * mounts). Once that device is unplugged, ready() is false and every
 * transfer fails until init() binds again -- even if another drive shows
 * up at the same USB address in the meantime. So a filesystem never writes
 * what it cached from one drive onto another one.
 *
 * With dev_addr == kAnyDevice, init() binds to the first mounted mass
 * storage device: convenient with a single drive. Use explicit addresses
 * (from USBHostMSC::onMount()) when several drives are attached via a hub.
 */
class USBHostMscDisk {
public:
  static constexpr uint8_t kAnyDevice = 0;
  static constexpr uint16_t kSectorSize = 512;

  USBHostMscDisk() = default;
  USBHostMscDisk(USBHostMSC &msc, uint8_t dev_addr = kAnyDevice,
                 uint8_t lun = 0) {
    begin(msc, dev_addr, lun);
  }

  /// Selects the device; the binding itself happens in init().
  void begin(USBHostMSC &msc, uint8_t dev_addr = kAnyDevice, uint8_t lun = 0) {
    _msc = &msc;
    _want = dev_addr;
    _lun = lun;
    _bound = false;
  }

  /// (Re)binds to the selected device. Fails if it is not mounted or does
  /// not use 512-byte blocks (the filesystems only support those).
  bool init() {
    _bound = false;
    if (_msc == nullptr) {
      return false;
    }
    uint8_t addr = _want;
    if (addr == kAnyDevice) {
      if (_msc->mountedDeviceCount() == 0) {
        return false;
      }
      addr = _msc->deviceAddress(0);
    }
    if (!_msc->isMounted(addr) || _msc->blockSize(addr, _lun) != kSectorSize ||
        _msc->blockCount(addr, _lun) == 0) {
      return false;
    }
    _addr = addr;
    _generation = _msc->mountGeneration(addr);
    _bound = true;
    return true;
  }

  /// Releases the binding: transfers fail until the next init().
  void end() { _bound = false; }

  /// True while the device bound by init() is still attached.
  bool ready() const {
    return _bound && _msc->isMounted(_addr) &&
           _msc->mountGeneration(_addr) == _generation;
  }

  /// USB address of the bound device, 0 if none.
  uint8_t deviceAddress() const { return ready() ? _addr : 0; }
  uint8_t lun() const { return _lun; }
  uint32_t sectorCount() const {
    return ready() ? _msc->blockCount(_addr, _lun) : 0;
  }

  bool read(uint32_t lba, void *buffer, uint32_t count) {
    if (!inRange(lba, count)) {
      return false;
    }
    uint8_t *p = (uint8_t *)buffer;
    while (count > 0) {
      uint16_t n = count > kMaxBlocksPerCall ? kMaxBlocksPerCall : (uint16_t)count;
      if (!_msc->readBlocks(_addr, lba, p, n, _lun)) {
        return false;
      }
      lba += n;
      p += (uint32_t)n * kSectorSize;
      count -= n;
    }
    return true;
  }

  bool write(uint32_t lba, const void *buffer, uint32_t count) {
    if (!inRange(lba, count)) {
      return false;
    }
    const uint8_t *p = (const uint8_t *)buffer;
    while (count > 0) {
      uint16_t n = count > kMaxBlocksPerCall ? kMaxBlocksPerCall : (uint16_t)count;
      if (!_msc->writeBlocks(_addr, lba, p, n, _lun)) {
        return false;
      }
      lba += n;
      p += (uint32_t)n * kSectorSize;
      count -= n;
    }
    return true;
  }

private:
  // USBHostMSC takes a 16-bit block count per SCSI command
  static constexpr uint16_t kMaxBlocksPerCall = 0x8000;

  bool inRange(uint32_t lba, uint32_t count) const {
    return ready() && (uint64_t)lba + count <= _msc->blockCount(_addr, _lun);
  }

  USBHostMSC *_msc = nullptr;
  uint8_t _want = kAnyDevice;
  uint8_t _addr = 0;
  uint8_t _lun = 0;
  uint32_t _generation = 0;
  bool _bound = false;
};
