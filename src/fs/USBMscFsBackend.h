#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

namespace usbmscfs {

class Backend;

/**
 * @brief Minimal POSIX-ish open file (or open directory) handle,
 * implemented once per filesystem backend (FAT, ext4, ...). The
 * platform-specific fs::FS/fs::File adapters are written once against
 * this interface and work with any backend.
 */
class BackendFile {
public:
  virtual ~BackendFile(); // unregisters from its Backend (defined below)

  virtual size_t read(uint8_t *buf, size_t len) = 0;
  virtual size_t write(const uint8_t *buf, size_t len) = 0;
  virtual bool seek(uint64_t pos) = 0;
  virtual uint64_t position() = 0;
  virtual uint64_t size() = 0;
  virtual void flush() = 0;
  virtual void close() = 0;

  virtual bool isDirectory() = 0;

  /// Fills in the next directory entry's basename (NUL-terminated into
  /// name_out, up to name_len bytes), its type and size. Returns false
  /// once there are no more entries. Only valid if isDirectory().
  virtual bool nextEntry(char *name_out, size_t name_len, bool &is_dir,
                          uint64_t &file_size) = 0;
  virtual void rewindDir() = 0;

private:
  friend class Backend;
  Backend *_owner = nullptr; // set by Backend::track(), cleared on detach
  BackendFile *_prev = nullptr;
  BackendFile *_next = nullptr;
};

/**
 * @brief Minimal mounted-filesystem interface, implemented once per
 * filesystem backend.
 */
class Backend {
public:
  virtual ~Backend() { detachAll(); }

  /// Shared liveness flag: true while this Backend exists. Adapters that
  /// keep a Backend& beyond the Backend's lifetime (open files, fs::FS
  /// impls) hold a copy and check it before every use, since a USB drive
  /// can be unplugged -- destroying the Backend -- while the sketch still
  /// holds File objects.
  std::shared_ptr<bool> aliveToken() const { return _alive; }

  virtual bool mount() = 0;

  /// True if the mounted filesystem is read-only (either requested, or
  /// forced by the backend, e.g. ext2/3 or a filesystem needing fsck).
  virtual bool isReadOnly() const { return false; }

  /// mode is a subset of fopen()-style strings: "r", "r+", "w", "w+", "a".
  /// Returns a heap-allocated handle (caller owns it and must delete it,
  /// typically after calling close()), or nullptr on failure.
  virtual BackendFile *open(const char *path, const char *mode) = 0;

  virtual bool exists(const char *path) = 0;
  virtual bool isDirectory(const char *path) = 0;
  virtual bool remove(const char *path) = 0;
  virtual bool rename(const char *path_from, const char *path_to) = 0;
  virtual bool mkdir(const char *path) = 0;
  virtual bool rmdir(const char *path) = 0;

protected:
  /// Registers a freshly opened file so it can be force-closed if the
  /// Backend goes away first. Returns f.
  BackendFile *track(BackendFile *f) {
    if (f) {
      f->_owner = this;
      f->_prev = nullptr;
      f->_next = _open_files;
      if (_open_files) {
        _open_files->_prev = f;
      }
      _open_files = f;
    }
    return f;
  }

  /// Closes every still-open file (while the derived class's filesystem
  /// state is still valid) and marks this Backend dead. Derived
  /// destructors must call this FIRST, before tearing down their state;
  /// it is idempotent.
  void detachAll() {
    while (_open_files) {
      BackendFile *f = _open_files;
      _open_files = f->_next;
      f->_owner = nullptr;
      f->_prev = f->_next = nullptr;
      f->close();
    }
    *_alive = false;
  }

private:
  friend class BackendFile;
  void untrack(BackendFile *f) {
    if (f->_prev) {
      f->_prev->_next = f->_next;
    } else if (_open_files == f) {
      _open_files = f->_next;
    }
    if (f->_next) {
      f->_next->_prev = f->_prev;
    }
    f->_owner = nullptr;
    f->_prev = f->_next = nullptr;
  }

  BackendFile *_open_files = nullptr;
  std::shared_ptr<bool> _alive = std::make_shared<bool>(true);
};

inline BackendFile::~BackendFile() {
  if (_owner) {
    _owner->untrack(this);
  }
}

} // namespace usbmscfs
