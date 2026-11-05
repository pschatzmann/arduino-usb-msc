// Desktop stand-in for USBHostMSC: the same device/block API that
// USBHostMscDisk uses, backed by disk-image files instead of TinyUSB. Only
// compiled with -DUSBHOSTMSC_HOST_TEST.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

class USBHostMSC {
public:
  ~USBHostMSC() { detach(); }

  /// "Plugs in" the image at path as device dev_addr (512-byte blocks).
  /// Like TinyUSB, a later drive may get the same address again.
  bool attach(uint8_t dev_addr, const char *path, uint32_t block_size = 512) {
    detach();
    _f = fopen(path, "r+b");
    if (!_f) {
      return false;
    }
    fseek(_f, 0, SEEK_END);
    _block_size = block_size;
    _blocks = (uint32_t)(ftell(_f) / block_size);
    _dev = dev_addr;
    _generation++;
    return true;
  }

  /// "Unplugs" the device: every later transfer fails.
  void detach() {
    if (_f) {
      fclose(_f);
      _f = nullptr;
    }
  }

  /// Makes every transfer fail after n more successful ones (-1 = never).
  void failAfter(int n) { _fail_after = n; }

  bool isMounted(uint8_t dev) const { return _f && dev == _dev; }
  uint32_t mountGeneration(uint8_t dev) const {
    return dev == _dev ? _generation : 0;
  }
  uint8_t mountedDeviceCount() const { return _f ? 1 : 0; }
  uint8_t deviceAddress(uint8_t index) const {
    return (_f && index == 0) ? _dev : 0;
  }
  uint32_t blockCount(uint8_t dev, uint8_t = 0) const {
    return isMounted(dev) ? _blocks : 0;
  }
  uint32_t blockSize(uint8_t dev, uint8_t = 0) const {
    return isMounted(dev) ? _block_size : 0;
  }

  bool readBlocks(uint8_t dev, uint32_t lba, void *buf, uint16_t n,
                  uint8_t = 0) {
    if (!ok(dev, lba, n)) {
      return false;
    }
    reads++;
    fseek(_f, (long)lba * _block_size, SEEK_SET);
    return fread(buf, _block_size, n, _f) == n;
  }
  bool writeBlocks(uint8_t dev, uint32_t lba, const void *buf, uint16_t n,
                   uint8_t = 0) {
    if (!ok(dev, lba, n)) {
      return false;
    }
    writes++;
    fseek(_f, (long)lba * _block_size, SEEK_SET);
    bool r = fwrite(buf, _block_size, n, _f) == n;
    fflush(_f);
    return r;
  }

  unsigned reads = 0, writes = 0; // successful SCSI commands
  uint16_t max_count = 0;         // largest block count of one command

private:
  bool ok(uint8_t dev, uint32_t lba, uint16_t n) {
    if (!isMounted(dev) || n == 0 || (uint64_t)lba + n > _blocks) {
      return false;
    }
    if (_fail_after == 0) {
      return false;
    }
    if (_fail_after > 0) {
      _fail_after--;
    }
    if (n > max_count) {
      max_count = n;
    }
    return true;
  }

  FILE *_f = nullptr;
  uint32_t _blocks = 0;
  uint32_t _block_size = 512;
  uint8_t _dev = 0;
  uint32_t _generation = 0;
  int _fail_after = -1;
};
