#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <FS.h>
#include <FSImpl.h>

#include "USBMscFsBackend.h"

namespace usbmscfs {

class EspFSImpl : public fs::FSImpl {
public:
  explicit EspFSImpl(Backend &backend)
      : _backend(backend), _alive(backend.aliveToken()) {}

  fs::FileImplPtr open(const char *path, const char *mode,
                        const bool create) override;
  bool exists(const char *path) override {
    return *_alive && _backend.exists(path);
  }
  bool rename(const char *pathFrom, const char *pathTo) override {
    return *_alive && _backend.rename(pathFrom, pathTo);
  }
  bool remove(const char *path) override { return *_alive && _backend.remove(path); }
  bool mkdir(const char *path) override { return *_alive && _backend.mkdir(path); }
  bool rmdir(const char *path) override { return *_alive && _backend.rmdir(path); }

private:
  Backend &_backend;
  std::shared_ptr<bool> _alive;
};

} // namespace usbmscfs

#endif
