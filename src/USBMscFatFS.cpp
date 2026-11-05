#include "USBMsc_config.h"
#if USBHOSTMSC_COMPILE
#include "USBMscFatFS.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)

#include "fs/USBMscFatBackend.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "fs/USBMscFsAdapterEsp32.h"
#elif defined(ARDUINO_ARCH_RP2040)
#include "fs/USBMscFsAdapterRp2040.h"
#endif

namespace {

bool looksLikeFat(USBMscPartition &part) {
  alignas(4) uint8_t buf[512];
  uint32_t bs = part.blockSize();
  if (bs == 0 || bs > sizeof(buf)) {
    return false;
  }
  if (!part.readBlocks(0, buf, 1)) {
    return false;
  }
  // Boot sector signature + a jmp opcode at offset 0.
  return buf[510] == 0x55 && buf[511] == 0xAA &&
         (buf[0] == 0xEB || buf[0] == 0xE9);
}

} // namespace

USBMscFatFS::USBMscFatFS() {}

USBMscFatFS::~USBMscFatFS() { reset(); }

bool USBMscFatFS::begin(USBHostMSC &msc, uint8_t dev_addr) {
  reset();

  USBMscPartitionTable::bindFirstMatching(msc, dev_addr, isFatPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscFatFS::begin(USBMscBlockDevice &dev) {
  reset();

  USBMscPartitionTable::bindFirstMatching(dev, isFatPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscFatFS::mountBound() {
  if (!looksLikeFat(_partition)) {
    return false;
  }

  _backend = new usbmscfs::FatBackend(_partition);
  if (!_backend->mount()) {
    delete _backend;
    _backend = nullptr;
    return false;
  }

#if defined(ARDUINO_ARCH_ESP32)
  _fs = new fs::FS(std::make_shared<usbmscfs::EspFSImpl>(*_backend));
#elif defined(ARDUINO_ARCH_RP2040)
  _fs = new fs::FS(std::make_shared<usbmscfs::RpFSImpl>(*_backend));
#endif
  return true;
}

void USBMscFatFS::reset() {
  delete _fs;
  _fs = nullptr;
  delete _backend;
  _backend = nullptr;
}

#endif // ARDUINO_ARCH_ESP32 || ARDUINO_ARCH_RP2040

#if defined(ARDUINO_ARCH_STM32)

#include "fs/USBMscFatBackend.h"

namespace {

bool looksLikeFat(USBMscPartition &part) {
  alignas(4) uint8_t buf[512];
  uint32_t bs = part.blockSize();
  if (bs == 0 || bs > sizeof(buf)) {
    return false;
  }
  if (!part.readBlocks(0, buf, 1)) {
    return false;
  }
  // Boot sector signature + a jmp opcode at offset 0.
  return buf[510] == 0x55 && buf[511] == 0xAA &&
         (buf[0] == 0xEB || buf[0] == 0xE9);
}

} // namespace

USBMscFatFS::USBMscFatFS() {}

USBMscFatFS::~USBMscFatFS() { reset(); }

bool USBMscFatFS::begin(USBHostMSC &msc, uint8_t dev_addr) {
  reset();

  USBMscPartitionTable::bindFirstMatching(msc, dev_addr, isFatPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscFatFS::begin(USBMscBlockDevice &dev) {
  reset();

  USBMscPartitionTable::bindFirstMatching(dev, isFatPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscFatFS::mountBound() {
  if (!looksLikeFat(_partition)) {
    return false;
  }

  auto *backend = new usbmscfs::FatBackend(_partition);
  if (!backend->mount()) {
    delete backend;
    return false;
  }
  _backend = backend;
  _ready = true;
  return true;
}

void USBMscFatFS::reset() {
  _ready = false;
  delete _backend;
  _backend = nullptr;
}

USBMscFile USBMscFatFS::open(const char *path, const char *mode) {
  if (!_backend) {
    return USBMscFile();
  }
  bool as_dir = exists(path) && _backend->isDirectory(path);
  usbmscfs::BackendFile *f = _backend->open(path, as_dir ? "d" : mode);
  if (!f) {
    return USBMscFile();
  }
  return USBMscFile(_backend, f, String(path));
}

bool USBMscFatFS::exists(const char *path) {
  return _backend && _backend->exists(path);
}
bool USBMscFatFS::remove(const char *path) {
  return _backend && _backend->remove(path);
}
bool USBMscFatFS::rename(const char *path_from, const char *path_to) {
  return _backend && _backend->rename(path_from, path_to);
}
bool USBMscFatFS::mkdir(const char *path) {
  return _backend && _backend->mkdir(path);
}
bool USBMscFatFS::rmdir(const char *path) {
  return _backend && _backend->rmdir(path);
}

#endif // ARDUINO_ARCH_STM32

#endif // USBHOSTMSC_COMPILE
