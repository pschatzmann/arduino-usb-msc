#include "USBMscFatBackend.h"

// TinyFATFS (library dependency) is header-only; this is the one and only
// translation unit in this library that includes it.
//
// TinyFATFS's fatfs-drivers.h includes driver/TinyUsbMscIO.h whenever
// USE_TINYUSB is defined; that driver exports a disk as a USB *device*
// (Adafruit_USBD_MSC), which doesn't exist in host-mode builds and which
// we never use. Hide the macro just for this include.
#ifdef USE_TINYUSB
#pragma push_macro("USE_TINYUSB")
#undef USE_TINYUSB
#define USBMSC_RESTORE_USE_TINYUSB
#endif
#include <fatfs.h>
#ifdef USBMSC_RESTORE_USE_TINYUSB
#pragma pop_macro("USE_TINYUSB")
#undef USBMSC_RESTORE_USE_TINYUSB
#endif

namespace usbmscfs {

namespace {

/// fatfs::IO adapter that reads/writes through a USBMscPartition.
class FatIO : public fatfs::IO {
public:
  explicit FatIO(USBMscPartition &part) : _part(part) {}

  fatfs::DSTATUS disk_initialize(BYTE) override { return fatfs::STA_CLEAR; }
  fatfs::DSTATUS disk_status(BYTE) override { return fatfs::STA_CLEAR; }
  fatfs::DRESULT disk_read(BYTE, BYTE *buff, LBA_t sector,
                            UINT count) override {
    return _part.readBlocks(sector, buff, count) ? fatfs::RES_OK
                                                   : fatfs::RES_ERROR;
  }
  fatfs::DRESULT disk_write(BYTE, const BYTE *buff, LBA_t sector,
                             UINT count) override {
    return _part.writeBlocks(sector, buff, count) ? fatfs::RES_OK
                                                    : fatfs::RES_ERROR;
  }
  fatfs::DRESULT disk_ioctl(BYTE, fatfs::ioctl_cmd_t cmd,
                             void *buff) override {
    switch (cmd) {
    case fatfs::CTRL_SYNC:
      return fatfs::RES_OK;
    case fatfs::GET_SECTOR_COUNT:
      *(LBA_t *)buff = _part.blockCount();
      return fatfs::RES_OK;
    case fatfs::GET_SECTOR_SIZE:
      *(WORD *)buff = (WORD)_part.blockSize();
      return fatfs::RES_OK;
    case fatfs::GET_BLOCK_SIZE:
      *(DWORD *)buff = 1;
      return fatfs::RES_OK;
    default:
      return fatfs::RES_PARERR;
    }
  }

private:
  USBMscPartition &_part;
};

class FatBackendFile : public BackendFile {
public:
  explicit FatBackendFile(fatfs::FatFs &fs) : _fs(fs) {}
  ~FatBackendFile() override { close(); }

  bool openFile(const char *path, BYTE flags) {
    _is_dir = false;
    _open = _fs.f_open(&_fil, path, flags) == fatfs::FR_OK;
    return _open;
  }
  bool openDir(const char *path) {
    _is_dir = true;
    _open = _fs.f_opendir(&_dir, path) == fatfs::FR_OK;
    return _open;
  }

  size_t read(uint8_t *buf, size_t len) override {
    if (_is_dir || !_open) {
      return 0;
    }
    UINT br = 0;
    _fs.f_read(&_fil, buf, len, &br);
    return br;
  }
  size_t write(const uint8_t *buf, size_t len) override {
    if (_is_dir || !_open) {
      return 0;
    }
    UINT bw = 0;
    _fs.f_write(&_fil, buf, len, &bw);
    return bw;
  }
  bool seek(uint64_t pos) override {
    if (_is_dir || !_open) {
      return false;
    }
    return _fs.f_lseek(&_fil, (FSIZE_t)pos) == fatfs::FR_OK;
  }
  uint64_t position() override {
    return (_is_dir || !_open) ? 0 : _fs.f_tell(&_fil);
  }
  uint64_t size() override {
    return (_is_dir || !_open) ? 0 : _fs.f_size(&_fil);
  }
  void flush() override {
    if (!_is_dir && _open) {
      _fs.f_sync(&_fil);
    }
  }
  void close() override {
    if (!_open) {
      return;
    }
    _open = false;
    if (_is_dir) {
      _fs.f_closedir(&_dir);
    } else {
      _fs.f_close(&_fil);
    }
  }
  bool isDirectory() override { return _is_dir; }

  bool nextEntry(char *name_out, size_t name_len, bool &is_dir,
                 uint64_t &file_size) override {
    if (!_is_dir || !_open) {
      return false;
    }
    fatfs::FILINFO fno;
    if (_fs.f_readdir(&_dir, &fno) != fatfs::FR_OK || fno.fname[0] == 0) {
      return false;
    }
    size_t i = 0;
    for (; fno.fname[i] != 0 && i < name_len - 1; i++) {
      name_out[i] = (char)fno.fname[i];
    }
    name_out[i] = 0;
    is_dir = (fno.fattrib & AM_DIR) != 0;
    file_size = fno.fsize;
    return true;
  }
  void rewindDir() override {
    if (_is_dir && _open) {
      _fs.f_readdir(&_dir, nullptr);
    }
  }

private:
  fatfs::FatFs &_fs;
  fatfs::FIL _fil;
  fatfs::DIR _dir;
  bool _is_dir = false;
  bool _open = false;
};

} // namespace

struct FatBackend::Impl {
  explicit Impl(USBMscPartition &part) : io(part), fs(io) {}
  FatIO io;
  fatfs::FatFs fs;
};

FatBackend::FatBackend(USBMscPartition &part) : _impl(new Impl(part)) {}
FatBackend::~FatBackend() {
  detachAll(); // close open files while _impl is still valid
  delete _impl;
}

bool FatBackend::mount() {
  if (_impl->io.mount(_impl->fs) != fatfs::FR_OK) {
    return false;
  }
  // f_mount(..., opt=0) is lazy: it succeeds without looking at the disk.
  // Opening the root directory forces FatFs to find and validate the
  // volume, so a non-FAT disk is rejected here instead of failing on the
  // first file access.
  fatfs::DIR root;
  if (_impl->fs.f_opendir(&root, "/") != fatfs::FR_OK) {
    return false;
  }
  _impl->fs.f_closedir(&root);
  return true;
}

BackendFile *FatBackend::open(const char *path, const char *mode) {
  FatBackendFile *f = new FatBackendFile(_impl->fs);
  if (mode[0] == 'd') { // internal convention: directory open
    if (!f->openDir(path)) {
      delete f;
      return nullptr;
    }
    return track(f);
  }

  const bool plus = mode[0] != 0 && mode[1] == '+';
  BYTE flags = FA_READ;
  if (mode[0] == 'w') {
    flags = FA_WRITE | FA_CREATE_ALWAYS | (plus ? FA_READ : 0);
  } else if (mode[0] == 'a') {
    flags = FA_WRITE | FA_OPEN_APPEND | (plus ? FA_READ : 0);
  } else if (mode[0] == 'r' && plus) {
    flags = FA_READ | FA_WRITE;
  }

  if (!f->openFile(path, flags)) {
    delete f;
    return nullptr;
  }
  return track(f);
}

bool FatBackend::exists(const char *path) {
  if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
    return true; // root always exists
  }
  fatfs::FILINFO fno;
  return _impl->fs.f_stat(path, &fno) == fatfs::FR_OK;
}

bool FatBackend::isDirectory(const char *path) {
  if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
    return true; // root
  }
  fatfs::FILINFO fno;
  if (_impl->fs.f_stat(path, &fno) != fatfs::FR_OK) {
    return false;
  }
  return (fno.fattrib & AM_DIR) != 0;
}
bool FatBackend::remove(const char *path) {
  return _impl->fs.f_unlink(path) == fatfs::FR_OK;
}
bool FatBackend::rename(const char *path_from, const char *path_to) {
  return _impl->fs.f_rename(path_from, path_to) == fatfs::FR_OK;
}
bool FatBackend::mkdir(const char *path) {
  return _impl->fs.f_mkdir(path) == fatfs::FR_OK;
}
bool FatBackend::rmdir(const char *path) {
  return _impl->fs.f_unlink(path) == fatfs::FR_OK;
}

} // namespace usbmscfs
