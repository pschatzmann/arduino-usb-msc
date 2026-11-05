#pragma once

/**
 * @file USBHostMscFatIO.h
 * @brief TinyFATFS driver for a USB flash drive: mount it with
 * fatfs::SDClass (FAT12/16/32, exFAT; MBR partitioned or not).
 *
 * @code
 * #include "USBHostMSC.h"
 * #include "USBHostMscFatIO.h"
 *
 * USBHostMSC msc;
 * USBHostMscFatIO usbIO(msc);   // first drive that is plugged in
 * fatfs::SDClass usbFat;
 *
 * void onMount(uint8_t) { usbFat.begin(usbIO); }
 * @endcode
 *
 * Unplugging is handled: open files fail, and the next access after a new
 * drive was plugged in mounts that drive (FatFs remounts by itself). For
 * fs::FS (ESP32 / RP2040) wrap the SDClass in fatfs::ArduinoFS
 * (fatfs-arduino-fs.h from TinyFATFS).
 */

#include <fatfs.h>

#include "USBHostMscDisk.h"

class USBHostMscFatIO : public fatfs::IO {
public:
  USBHostMscFatIO() = default;
  explicit USBHostMscFatIO(USBHostMSC &msc,
                           uint8_t dev_addr = USBHostMscDisk::kAnyDevice,
                           uint8_t lun = 0)
      : _disk(msc, dev_addr, lun) {}

  /// Selects the device (kAnyDevice: the first one that is mounted).
  void begin(USBHostMSC &msc, uint8_t dev_addr = USBHostMscDisk::kAnyDevice,
             uint8_t lun = 0) {
    _disk.begin(msc, dev_addr, lun);
  }

  USBHostMscDisk &disk() { return _disk; }

  /// Mounts immediately (not lazily, as the default does), so that
  /// SDClass::begin() fails if there is no FAT/exFAT volume on the drive.
  fatfs::FRESULT mount(fatfs::FatFs &fs, BYTE pdrv = 0) override {
    char path[6];
    snprintf(path, sizeof(path), "%d:", pdrv);
    return fs.f_mount(&fatfs, path, 1);
  }

  fatfs::DSTATUS disk_initialize(BYTE pdrv) override {
    if (pdrv != 0 || !_disk.init()) {
      return gone();
    }
    return fatfs::STA_CLEAR;
  }
  fatfs::DSTATUS disk_status(BYTE pdrv) override {
    return (pdrv == 0 && _disk.ready()) ? fatfs::STA_CLEAR : gone();
  }
  fatfs::DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector,
                           UINT count) override {
    if (pdrv != 0 || !_disk.ready()) {
      return fatfs::RES_NOTRDY;
    }
    return _disk.read(sector, buff, count) ? fatfs::RES_OK : fatfs::RES_ERROR;
  }
  fatfs::DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector,
                            UINT count) override {
    if (pdrv != 0 || !_disk.ready()) {
      return fatfs::RES_NOTRDY;
    }
    return _disk.write(sector, buff, count) ? fatfs::RES_OK : fatfs::RES_ERROR;
  }
  fatfs::DRESULT disk_ioctl(BYTE pdrv, fatfs::ioctl_cmd_t cmd,
                            void *buff) override {
    if (pdrv != 0 || !_disk.ready()) {
      return fatfs::RES_NOTRDY;
    }
    switch (cmd) {
    case fatfs::CTRL_SYNC: // every write is already sent to the drive
      return fatfs::RES_OK;
    case fatfs::GET_SECTOR_COUNT:
      *(LBA_t *)buff = _disk.sectorCount();
      return fatfs::RES_OK;
    case fatfs::GET_SECTOR_SIZE:
      *(WORD *)buff = USBHostMscDisk::kSectorSize;
      return fatfs::RES_OK;
    case fatfs::GET_BLOCK_SIZE:
      *(DWORD *)buff = 1;
      return fatfs::RES_OK;
    default:
      return fatfs::RES_PARERR;
    }
  }

private:
  static fatfs::DSTATUS gone() {
    return (fatfs::DSTATUS)(fatfs::STA_NOINIT | fatfs::STA_NODISK);
  }
  USBHostMscDisk _disk;
};
