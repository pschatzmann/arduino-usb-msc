// Desktop stand-in for USBHostMSC: same block-level API that
// USBMscPartition and the filesystem backends use, backed by a disk-image
// file instead of TinyUSB. Only compiled with -DUSBHOSTMSC_HOST_TEST.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

class USBMscFSBackend;

class USBHostMSC {
public:
  explicit USBHostMSC(USBMscFSBackend * = nullptr) {}
  ~USBHostMSC() { detach(); }

  /// "Plugs in" the image at path as device dev_addr (512-byte blocks).
  bool attach(uint8_t dev_addr, const char *path) {
    detach();
    _f = fopen(path, "r+b");
    if (!_f) {
      return false;
    }
    fseek(_f, 0, SEEK_END);
    _blocks = (uint32_t)(ftell(_f) / 512);
    _dev = dev_addr;
    return true;
  }

  /// "Unplugs" the device: every later transfer fails, like the real
  /// USBHostMSC once the device has gone.
  void detach() {
    if (_f) {
      fclose(_f);
      _f = nullptr;
    }
  }

  /// Makes every transfer fail after n more successful ones (-1 = never).
  void failAfter(int n) { _fail_after = n; }

  uint32_t blockCount(uint8_t dev, uint8_t = 0) const {
    return (_f && dev == _dev) ? _blocks : 0;
  }
  uint32_t blockSize(uint8_t dev, uint8_t = 0) const {
    return (_f && dev == _dev) ? 512 : 0;
  }

  bool readBlocks(uint8_t dev, uint32_t lba, void *buf, uint16_t n,
                  uint8_t = 0) {
    if (!ok(dev, lba, n)) {
      return false;
    }
    fseek(_f, (long)lba * 512, SEEK_SET);
    return fread(buf, 512, n, _f) == n;
  }
  bool writeBlocks(uint8_t dev, uint32_t lba, const void *buf, uint16_t n,
                   uint8_t = 0) {
    if (!ok(dev, lba, n)) {
      return false;
    }
    fseek(_f, (long)lba * 512, SEEK_SET);
    bool r = fwrite(buf, 512, n, _f) == n;
    fflush(_f);
    return r;
  }

  unsigned reads = 0, writes = 0; // for tests that care

private:
  bool ok(uint8_t dev, uint32_t lba, uint16_t n) {
    if (!_f || dev != _dev || (uint64_t)lba + n > _blocks) {
      return false;
    }
    if (_fail_after == 0) {
      return false;
    }
    if (_fail_after > 0) {
      _fail_after--;
    }
    return true;
  }

  FILE *_f = nullptr;
  uint32_t _blocks = 0;
  uint8_t _dev = 0;
  int _fail_after = -1;
};
