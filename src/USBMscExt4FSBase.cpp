#include "USBMsc_config.h"
#if USBHOSTMSC_COMPILE
#include "USBMscExt4FSBase.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040)

#include "fs/USBMscExt4Backend.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "fs/USBMscFsAdapterEsp32.h"
#elif defined(ARDUINO_ARCH_RP2040)
#include "fs/USBMscFsAdapterRp2040.h"
#endif

namespace {

bool looksLikeExt(USBMscPartition &part) {
  // TinyEXT4 addresses the device in 512-byte sectors and probes for the
  // ext superblock itself (whole device first, then MBR/GPT partitions),
  // so all that's left to rule out here is a different sector size.
  return part.blockSize() == 512;
}

} // namespace

USBMscExt4FSBase::~USBMscExt4FSBase() { reset(); }

bool USBMscExt4FSBase::begin(USBHostMSC &msc, uint8_t dev_addr) {
  reset();

  USBMscPartitionTable::bindFirstMatching(msc, dev_addr, isLinuxPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscExt4FSBase::begin(USBMscBlockDevice &dev) {
  reset();

  USBMscPartitionTable::bindFirstMatching(dev, isLinuxPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscExt4FSBase::mountBound() {
  if (!looksLikeExt(_partition)) {
    return false;
  }

  _backend = new usbmscfs::Ext4Backend(_partition, _read_only);
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

bool USBMscExt4FSBase::readOnly() const {
  return _backend && _backend->isReadOnly();
}

void USBMscExt4FSBase::reset() {
  delete _fs;
  _fs = nullptr;
  delete _backend;
  _backend = nullptr;
}

#endif // ARDUINO_ARCH_ESP32 || ARDUINO_ARCH_RP2040

#if defined(ARDUINO_ARCH_STM32)

#include "fs/USBMscExt4Backend.h"

namespace {

bool looksLikeExt(USBMscPartition &part) {
  // TinyEXT4 addresses the device in 512-byte sectors and probes for the
  // ext superblock itself (whole device first, then MBR/GPT partitions),
  // so all that's left to rule out here is a different sector size.
  return part.blockSize() == 512;
}

} // namespace

USBMscExt4FSBase::~USBMscExt4FSBase() { reset(); }

bool USBMscExt4FSBase::begin(USBHostMSC &msc, uint8_t dev_addr) {
  reset();

  USBMscPartitionTable::bindFirstMatching(msc, dev_addr, isLinuxPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscExt4FSBase::begin(USBMscBlockDevice &dev) {
  reset();

  USBMscPartitionTable::bindFirstMatching(dev, isLinuxPartitionType,
                                           _partition);
  return mountBound();
}

bool USBMscExt4FSBase::mountBound() {
  if (!looksLikeExt(_partition)) {
    return false;
  }

  auto *backend = new usbmscfs::Ext4Backend(_partition, _read_only);
  if (!backend->mount()) {
    delete backend;
    return false;
  }
  _backend = backend;
  _ready = true;
  return true;
}

bool USBMscExt4FSBase::readOnly() const {
  return _backend && _backend->isReadOnly();
}

void USBMscExt4FSBase::reset() {
  _ready = false;
  delete _backend;
  _backend = nullptr;
}

USBMscFile USBMscExt4FSBase::open(const char *path, const char *mode) {
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

bool USBMscExt4FSBase::exists(const char *path) {
  return _backend && _backend->exists(path);
}
bool USBMscExt4FSBase::remove(const char *path) {
  return _backend && _backend->remove(path);
}
bool USBMscExt4FSBase::rename(const char *path_from, const char *path_to) {
  return _backend && _backend->rename(path_from, path_to);
}
bool USBMscExt4FSBase::mkdir(const char *path) {
  return _backend && _backend->mkdir(path);
}
bool USBMscExt4FSBase::rmdir(const char *path) {
  return _backend && _backend->rmdir(path);
}

#endif // ARDUINO_ARCH_STM32

#endif // USBHOSTMSC_COMPILE
