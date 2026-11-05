#pragma once

// STM32 only -- ESP32/RP2040 use the core's own fs::File instead. This
// whole header compiles to nothing elsewhere.
#if defined(ARDUINO_ARCH_STM32)

#include <Arduino.h>

#include <memory>

#include "fs/USBMscFsBackend.h"

/**
 * @brief STM32's file/directory handle, returned by USBMscFatFS::open()
 * (and USBMscFatFS::fs().open(), for cross-platform sketch parity with
 * ESP32/RP2040's fs::File). Mimics the shape of STM32SD's own File class
 * (open/read/write/isDirectory/openNextFile/name/size/close) -- deliberately
 * *not* named `File`/`SDClass` like STM32SD does, so a sketch can use both
 * an actual SD card (via STM32SD) and a USB drive (via this library) at
 * the same time without a name collision.
 *
 * Copyable like fs::File: the underlying handle is reference-counted, so
 * copies (e.g. `File f = dir.openNextFile();`) share the same open file
 * safely.
 */
class USBMscFile : public Stream {
public:
  USBMscFile() = default;
  USBMscFile(usbmscfs::Backend *backend, usbmscfs::BackendFile *f,
             const String &path)
      : _backend(backend), _alive(backend ? backend->aliveToken() : std::shared_ptr<bool>()),
        _f(f), _path(path) {
    int slash = _path.lastIndexOf('/');
    _name = slash >= 0 ? _path.substring(slash + 1) : _path;
  }

  // Stream / Print
  int available() override {
    if (!_f) {
      return 0;
    }
    int remaining = (int)(_f->size() - _f->position());
    return remaining + (_has_peek ? 1 : 0);
  }
  int read() override {
    if (_has_peek) {
      _has_peek = false;
      return _peek_byte;
    }
    uint8_t b;
    return (_f && _f->read(&b, 1) == 1) ? b : -1;
  }
  int peek() override {
    if (_has_peek) {
      return _peek_byte;
    }
    uint8_t b;
    if (_f && _f->read(&b, 1) == 1) {
      _peek_byte = b;
      _has_peek = true;
      return b;
    }
    return -1;
  }
  void flush() override {
    if (_f) {
      _f->flush();
    }
  }
  size_t write(uint8_t b) override { return _f ? _f->write(&b, 1) : 0; }
  size_t write(const uint8_t *buf, size_t len) override {
    return _f ? _f->write(buf, len) : 0;
  }
  using Print::write;

  /// Bulk read convenience (not part of Stream/Print).
  size_t read(uint8_t *buf, size_t len) {
    if (!_f || len == 0) {
      return 0;
    }
    size_t n = 0;
    if (_has_peek) {
      buf[0] = _peek_byte;
      _has_peek = false;
      n = 1;
      if (len == 1) {
        return 1;
      }
    }
    n += _f->read(buf + n, len - n);
    return n;
  }

  bool seek(uint32_t pos) {
    _has_peek = false;
    return _f && _f->seek(pos);
  }
  size_t position() const { return _f ? (size_t)_f->position() : 0; }
  size_t size() const { return _f ? (size_t)_f->size() : 0; }
  bool isDirectory() const { return _f && _f->isDirectory(); }
  const char *name() const { return _name.c_str(); }
  const char *path() const { return _path.c_str(); }
  void close() {
    if (_f) {
      _f->close();
    }
  }
  operator bool() const { return _f != nullptr; }

  /// Next directory entry, opened for reading. Only valid if
  /// isDirectory(). Returns a default-constructed (falsy) USBMscFile once
  /// there are no more entries.
  USBMscFile openNextFile() {
    if (!_f || !_f->isDirectory() || !_backend || !_alive || !*_alive) {
      return USBMscFile();
    }
    char child_name[128];
    bool is_dir;
    uint64_t fsize;
    if (!_f->nextEntry(child_name, sizeof(child_name), is_dir, fsize)) {
      return USBMscFile();
    }
    String child_path = _path;
    if (!child_path.endsWith("/")) {
      child_path += "/";
    }
    child_path += child_name;
    usbmscfs::BackendFile *child =
        _backend->open(child_path.c_str(), is_dir ? "d" : "r");
    if (!child) {
      return USBMscFile();
    }
    return USBMscFile(_backend, child, child_path);
  }
  void rewindDirectory() {
    if (_f) {
      _f->rewindDir();
    }
  }

private:
  usbmscfs::Backend *_backend = nullptr;
  std::shared_ptr<bool> _alive; // false once the drive is gone
  std::shared_ptr<usbmscfs::BackendFile> _f;
  String _path;
  String _name;
  uint8_t _peek_byte = 0;
  bool _has_peek = false;
};

#endif // ARDUINO_ARCH_STM32
