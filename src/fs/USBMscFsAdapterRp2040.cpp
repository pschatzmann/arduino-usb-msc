#include "USBMscFsAdapterRp2040.h"

#if defined(ARDUINO_ARCH_RP2040)

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

const char *modeFromFlags(fs::OpenMode openMode, fs::AccessMode accessMode) {
  if (openMode & fs::OM_APPEND) {
    return "a";
  }
  if ((openMode & fs::OM_CREATE) || (openMode & fs::OM_TRUNCATE)) {
    return (accessMode == fs::AM_RW) ? "w+" : "w";
  }
  return (accessMode == fs::AM_RW) ? "r+" : "r";
}

} // namespace

class RpFileImpl : public fs::FileImpl {
public:
  RpFileImpl(Backend &backend, BackendFile *f, const String &path)
      : _backend(backend), _alive(backend.aliveToken()), _f(f),
        _path(path) {
    int slash = _path.lastIndexOf('/');
    _name = slash >= 0 ? _path.substring(slash + 1) : _path;
  }
  ~RpFileImpl() override {
    if (_f) {
      _f->close();
      delete _f;
    }
  }

  size_t write(const uint8_t *buf, size_t size) override {
    return _f ? _f->write(buf, size) : 0;
  }
  int read(uint8_t *buf, size_t size) override {
    return _f ? (int)_f->read(buf, size) : -1;
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
  bool truncate(uint32_t) override { return false; }
  void close() override {
    if (_f) {
      _f->close();
    }
  }
  const char *name() const override { return _name.c_str(); }
  const char *fullName() const override { return _path.c_str(); }
  bool isFile() const override { return _f && !_f->isDirectory(); }
  bool isDirectory() const override { return _f && _f->isDirectory(); }

private:
  Backend &_backend;
  std::shared_ptr<bool> _alive; // false once the drive is gone
  BackendFile *_f;
  String _path;
  String _name;
};

class RpDirImpl : public fs::DirImpl {
public:
  RpDirImpl(Backend &backend, BackendFile *dir, const String &dirPath)
      : _backend(backend), _alive(backend.aliveToken()), _dir(dir),
        _dirPath(dirPath) {}
  ~RpDirImpl() override {
    if (_dir) {
      _dir->close();
      delete _dir;
    }
  }

  fs::FileImplPtr openFile(fs::OpenMode openMode,
                            fs::AccessMode accessMode) override {
    if (!*_alive || !_has_entry) {
      return fs::FileImplPtr();
    }
    String child_path = joinPath(_dirPath, _cur_name);
    const char *mode =
        _cur_is_dir ? "d" : modeFromFlags(openMode, accessMode);
    BackendFile *f = _backend.open(child_path.c_str(), mode);
    if (!f) {
      return fs::FileImplPtr();
    }
    return std::make_shared<RpFileImpl>(_backend, f, child_path);
  }
  const char *fileName() override { return _cur_name; }
  size_t fileSize() override { return (size_t)_cur_size; }
  bool isFile() const override { return _has_entry && !_cur_is_dir; }
  bool isDirectory() const override { return _has_entry && _cur_is_dir; }
  bool next() override {
    if (!*_alive) {
      _has_entry = false;
      return false;
    }
    _has_entry = _dir->nextEntry(_cur_name, sizeof(_cur_name), _cur_is_dir,
                                  _cur_size);
    return _has_entry;
  }
  bool rewind() override {
    if (*_alive) {
      _dir->rewindDir();
    }
    _has_entry = false;
    return true;
  }

private:
  Backend &_backend;
  std::shared_ptr<bool> _alive;
  BackendFile *_dir;
  String _dirPath;
  char _cur_name[128] = {0};
  bool _cur_is_dir = false;
  uint64_t _cur_size = 0;
  bool _has_entry = false;
};

bool RpFSImpl::info(fs::FSInfo &info) {
  memset(&info, 0, sizeof(info));
  info.blockSize = 512;
  info.pageSize = 512;
  info.maxOpenFiles = 8;
  info.maxPathLength = 255;
  return false; // total/used byte counts not tracked by the backend
}

fs::FileImplPtr RpFSImpl::open(const char *path, fs::OpenMode openMode,
                                fs::AccessMode accessMode) {
  if (!*_alive) {
    return fs::FileImplPtr();
  }
  bool as_dir = _backend.isDirectory(path);
  const char *mode = as_dir ? "d" : modeFromFlags(openMode, accessMode);
  BackendFile *f = _backend.open(path, mode);
  if (!f) {
    return fs::FileImplPtr();
  }
  return std::make_shared<RpFileImpl>(_backend, f, String(path));
}

fs::DirImplPtr RpFSImpl::openDir(const char *path) {
  if (!*_alive) {
    return fs::DirImplPtr();
  }
  BackendFile *d = _backend.open(path, "d");
  if (!d) {
    return fs::DirImplPtr();
  }
  return std::make_shared<RpDirImpl>(_backend, d, String(path));
}

bool RpFSImpl::stat(const char *path, fs::FSStat *st) {
  if (!*_alive) {
    return false;
  }
  bool is_dir = _backend.isDirectory(path);
  BackendFile *f = _backend.open(path, is_dir ? "d" : "r");
  if (!f) {
    return false;
  }
  st->size = (size_t)f->size();
  st->blocksize = 512;
  st->ctime = 0;
  st->atime = 0;
  st->isDir = is_dir;
  f->close();
  delete f;
  return true;
}

} // namespace usbmscfs

#endif
