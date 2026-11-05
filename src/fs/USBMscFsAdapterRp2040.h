#pragma once

#if defined(ARDUINO_ARCH_RP2040)

#include <FS.h>
#include <FSImpl.h>

#include "USBMscFsBackend.h"

namespace usbmscfs {

class RpFSImpl : public fs::FSImpl {
public:
  explicit RpFSImpl(Backend &backend)
      : _backend(backend), _alive(backend.aliveToken()) {}

  bool setConfig(const fs::FSConfig &) override { return true; }
  bool begin() override { return true; } // already mounted by USBMscFS::begin()
  void end() override {}
  bool format() override { return false; }
  bool info(fs::FSInfo &info) override;

  fs::FileImplPtr open(const char *path, fs::OpenMode openMode,
                        fs::AccessMode accessMode) override;
  bool exists(const char *path) override {
    return *_alive && _backend.exists(path);
  }
  fs::DirImplPtr openDir(const char *path) override;
  bool rename(const char *pathFrom, const char *pathTo) override {
    return *_alive && _backend.rename(pathFrom, pathTo);
  }
  bool remove(const char *path) override { return *_alive && _backend.remove(path); }
  bool mkdir(const char *path) override { return *_alive && _backend.mkdir(path); }
  bool rmdir(const char *path) override { return *_alive && _backend.rmdir(path); }
  bool stat(const char *path, fs::FSStat *st) override;

private:
  Backend &_backend;
  std::shared_ptr<bool> _alive;
};

} // namespace usbmscfs

#endif
