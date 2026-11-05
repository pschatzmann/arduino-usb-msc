#include "USBMscExt4Backend.h"

#include <string.h>

// TinyEXT4 is header-only; this is the one and only translation unit in
// this library that includes it (same pattern as TinyFATFS).
#include <TinyEXT4.h>

namespace usbmscfs {

namespace {

/// tinyext4::IO adapter that reads/writes through a USBMscPartition.
/// TinyEXT4 always addresses drive 0 in 512-byte sectors.
class UsbIO : public tinyext4::IO {
public:
  explicit UsbIO(USBMscPartition &part) : _part(part) {}

  tinyext4::DSTATUS disk_initialize(tinyext4::BYTE) override {
    return _part.blockSize() == 512 ? tinyext4::STA_CLEAR
                                    : tinyext4::STA_NOINIT;
  }
  tinyext4::DSTATUS disk_status(tinyext4::BYTE) override {
    return _part.blockSize() == 512 ? tinyext4::STA_CLEAR
                                    : tinyext4::STA_NOINIT;
  }
  tinyext4::DRESULT disk_read(tinyext4::BYTE, tinyext4::BYTE *buff,
                              tinyext4::LBA_t sector,
                              tinyext4::UINT count) override {
    return _part.readBlocks(sector, buff, (uint16_t)count) ? tinyext4::RES_OK
                                                           : tinyext4::RES_ERROR;
  }
  tinyext4::DRESULT disk_write(tinyext4::BYTE, const tinyext4::BYTE *buff,
                               tinyext4::LBA_t sector,
                               tinyext4::UINT count) override {
    return _part.writeBlocks(sector, buff, (uint16_t)count)
               ? tinyext4::RES_OK
               : tinyext4::RES_ERROR;
  }
  tinyext4::DRESULT disk_ioctl(tinyext4::BYTE, tinyext4::ioctl_cmd_t cmd,
                               void *buff) override {
    switch (cmd) {
    case tinyext4::CTRL_SYNC:
      return tinyext4::RES_OK; // every write is synchronous
    case tinyext4::GET_SECTOR_COUNT:
      if (buff) {
        *(tinyext4::LBA_t *)buff = _part.blockCount();
      }
      return tinyext4::RES_OK;
    case tinyext4::GET_SECTOR_SIZE:
      if (buff) {
        *(tinyext4::WORD *)buff = 512;
      }
      return tinyext4::RES_OK;
    case tinyext4::GET_BLOCK_SIZE:
      if (buff) {
        *(tinyext4::DWORD *)buff = 1;
      }
      return tinyext4::RES_OK;
    default:
      return tinyext4::RES_PARERR;
    }
  }

private:
  USBMscPartition &_part;
};

class Ext4BackendFile : public BackendFile {
public:
  explicit Ext4BackendFile(tinyext4::File f) : _file(f) {
    _open = (bool)_file;
    _is_dir = _open && _file.isDirectory();
  }
  ~Ext4BackendFile() override { close(); }

  size_t read(uint8_t *buf, size_t len) override {
    return (_is_dir || !_open) ? 0 : _file.read(buf, len);
  }
  size_t write(const uint8_t *buf, size_t len) override {
    // Returns 0 if the filesystem was mounted read-only.
    return (_is_dir || !_open) ? 0 : _file.write(buf, len);
  }
  bool seek(uint64_t pos) override {
    return !_is_dir && _open && _file.seek(pos);
  }
  uint64_t position() override {
    return (_is_dir || !_open) ? 0 : _file.position();
  }
  uint64_t size() override { return (_is_dir || !_open) ? 0 : _file.size(); }
  void flush() override {
    if (!_is_dir && _open) {
      _file.flush();
    }
  }
  void close() override {
    if (!_open) {
      return;
    }
    _open = false;
    _file.close(); // writes the inode and cached data
  }
  bool isDirectory() override { return _is_dir; }

  bool nextEntry(char *name_out, size_t name_len, bool &is_dir,
                 uint64_t &file_size) override {
    if (!_is_dir || !_open || name_len == 0) {
      return false;
    }
    // openNextFile() skips "." and ".." and loads the entry's inode, so
    // the size comes for free.
    tinyext4::File child = _file.openNextFile();
    if (!child) {
      return false;
    }
    strncpy(name_out, child.name(), name_len - 1);
    name_out[name_len - 1] = 0;
    is_dir = child.isDirectory();
    file_size = child.size();
    return true;
  }
  void rewindDir() override {
    if (_is_dir && _open) {
      _file.rewindDirectory();
    }
  }

private:
  tinyext4::File _file;
  bool _is_dir = false;
  bool _open = false;
};

} // namespace

struct Ext4Backend::Impl {
  Impl(USBMscPartition &part, bool read_only_in)
      : io(part), read_only(read_only_in) {}

  UsbIO io;
  tinyext4::Ext4FS fs;
  bool read_only;
};

Ext4Backend::Ext4Backend(USBMscPartition &part, bool read_only)
    : _impl(new Impl(part, read_only)) {}

Ext4Backend::~Ext4Backend() {
  detachAll(); // flush + close open files while the mount is still valid
  delete _impl; // ~Ext4FS() writes everything and marks the fs clean
}

bool Ext4Backend::mount() {
  // partition = -1: TinyEXT4 first tries the whole (already
  // partition-bound) device, then scans MBR/GPT for the first ext partition.
  return _impl->fs.begin(_impl->io, _impl->read_only, -1);
}

bool Ext4Backend::isReadOnly() const { return _impl->fs.isReadOnly(); }

BackendFile *Ext4Backend::open(const char *path, const char *mode) {
  bool want_dir = mode[0] == 'd'; // internal convention: directory open
  tinyext4::File file = _impl->fs.open(path, want_dir ? "r" : mode);
  if (!file) {
    return nullptr;
  }
  if (want_dir && !file.isDirectory()) {
    file.close();
    return nullptr;
  }
  return track(new Ext4BackendFile(file));
}

bool Ext4Backend::exists(const char *path) {
  if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
    return true; // root
  }
  return _impl->fs.exists(path);
}

bool Ext4Backend::isDirectory(const char *path) {
  if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
    return true; // root
  }
  tinyext4::FileInfo info;
  return _impl->fs.stat(path, info) && (info.mode & 0xF000) == 0x4000;
}

bool Ext4Backend::remove(const char *path) { return _impl->fs.remove(path); }
bool Ext4Backend::rename(const char *path_from, const char *path_to) {
  return _impl->fs.rename(path_from, path_to);
}
bool Ext4Backend::mkdir(const char *path) { return _impl->fs.mkdir(path); }
bool Ext4Backend::rmdir(const char *path) { return _impl->fs.rmdir(path); }

} // namespace usbmscfs
