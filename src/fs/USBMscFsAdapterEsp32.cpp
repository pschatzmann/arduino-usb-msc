#include "../USBMsc_config.h"
#if USBHOSTMSC_COMPILE
#include "USBMscFsAdapterEsp32.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <string.h>

namespace usbmscfs {

namespace {

String joinPath(const String &dir, const char *name) {
  String p = dir;
  if (!p.endsWith("/")) {
    p += "/";
  }
  p += name;
  return p;
}

} // namespace

class EspFileImpl : public fs::FileImpl {
public:
  EspFileImpl(Backend &backend, BackendFile *f, const String &path)
      : _backend(backend), _alive(backend.aliveToken()), _f(f),
        _path(path) {
    int slash = _path.lastIndexOf('/');
    _name = slash >= 0 ? _path.substring(slash + 1) : _path;
  }
  ~EspFileImpl() override {
    if (_f) {
      _f->close();
      delete _f;
    }
  }

  size_t write(const uint8_t *buf, size_t size) override {
    return _f ? _f->write(buf, size) : 0;
  }
  size_t read(uint8_t *buf, size_t size) override {
    return _f ? _f->read(buf, size) : 0;
  }
  void flush() override {
    if (_f) {
      _f->flush();
    }
  }
  bool seek(uint32_t pos, SeekMode mode) override {
    if (!_f) {
      return false;
    }
    uint64_t target = pos;
    if (mode == SeekCur) {
      target = _f->position() + pos;
    } else if (mode == SeekEnd) {
      // Arduino semantics: pos counts backwards from the end.
      if (pos > _f->size()) {
        return false;
      }
      target = _f->size() - pos;
    }
    return _f->seek(target);
  }
  size_t position() const override { return _f ? (size_t)_f->position() : 0; }
  size_t size() const override { return _f ? (size_t)_f->size() : 0; }
  bool setBufferSize(size_t) override { return true; }
  void close() override {
    if (_f) {
      _f->close();
    }
  }
  time_t getLastWrite() override { return 0; }
  const char *path() const override { return _path.c_str(); }
  const char *name() const override { return _name.c_str(); }
  boolean isDirectory(void) override { return _f && _f->isDirectory(); }

  fs::FileImplPtr openNextFile(const char *mode) override {
    if (!*_alive || !_f || !_f->isDirectory()) {
      return fs::FileImplPtr();
    }
    char child_name[128];
    bool is_dir;
    uint64_t fsize;
    if (!_f->nextEntry(child_name, sizeof(child_name), is_dir, fsize)) {
      return fs::FileImplPtr();
    }
    String child_path = joinPath(_path, child_name);
    BackendFile *child =
        _backend.open(child_path.c_str(), is_dir ? "d" : mode);
    if (!child) {
      return fs::FileImplPtr();
    }
    return std::make_shared<EspFileImpl>(_backend, child, child_path);
  }

  boolean seekDir(long position) override {
    if (!_f || !_f->isDirectory()) {
      return false;
    }
    _f->rewindDir();
    char tmp[128];
    bool is_dir;
    uint64_t fsize;
    for (long i = 0; i < position; i++) {
      if (!_f->nextEntry(tmp, sizeof(tmp), is_dir, fsize)) {
        return false;
      }
    }
    return true;
  }
  String getNextFileName(void) override { return getNextFileName(nullptr); }
  String getNextFileName(bool *isDir) override {
    if (!_f || !_f->isDirectory()) {
      return String();
    }
    char child_name[128];
    bool is_dir;
    uint64_t fsize;
    if (!_f->nextEntry(child_name, sizeof(child_name), is_dir, fsize)) {
      return String();
    }
    if (isDir) {
      *isDir = is_dir;
    }
    return String(child_name);
  }
  void rewindDirectory(void) override {
    if (_f) {
      _f->rewindDir();
    }
  }
  operator bool() override { return _f != nullptr; }

private:
  Backend &_backend;
  std::shared_ptr<bool> _alive; // false once the drive is gone
  BackendFile *_f;
  String _path;
  String _name;
};

fs::FileImplPtr EspFSImpl::open(const char *path, const char *mode,
                                 const bool create) {
  (void)create;
  if (!*_alive) {
    return fs::FileImplPtr();
  }
  bool as_dir = _backend.isDirectory(path);
  BackendFile *f = _backend.open(path, as_dir ? "d" : mode);
  if (!f) {
    return fs::FileImplPtr();
  }
  return std::make_shared<EspFileImpl>(_backend, f, String(path));
}

} // namespace usbmscfs

#endif

#endif // USBHOSTMSC_COMPILE
