#pragma once

/**
 * @file USBHostMscExt4IO.h
 * @brief TinyEXT4 driver for a USB flash drive: mount it with
 * tinyext4::Ext4FS (ext2/3/4; whole disk, MBR or GPT partitioned).
 *
 * @code
 * #include "USBHostMSC.h"
 * #include "USBHostMscExt4IO.h"
 *
 * USBHostMSC msc;
 * USBHostMscExt4IO usbIO(msc);   // first drive that is plugged in
 * tinyext4::Ext4FS usbExt4;
 *
 * void onMount(uint8_t) { usbExt4.begin(usbIO); }
 * void onUnmount(uint8_t) { usbExt4.end(); }
 * @endcode
 *
 * Unplugging is handled: every transfer fails until Ext4FS::begin() is
 * called again, so nothing cached from the old drive reaches a new one,
 * and Files of the old mount stay invalid. TinyEXT4 writes no journal:
 * pulling a drive during a write can leave it needing e2fsck. For fs::FS
 * (ESP32 / RP2040) wrap the Ext4FS in tinyext4::ArduinoFS
 * (tinyext4/ArduinoFS.h from TinyEXT4).
 */

#include <TinyEXT4.h>

#include "USBHostMscDisk.h"

class USBHostMscExt4IO : public tinyext4::IO {
public:
  USBHostMscExt4IO() = default;
  explicit USBHostMscExt4IO(USBHostMSC &msc,
                            uint8_t dev_addr = USBHostMscDisk::kAnyDevice,
                            uint8_t lun = 0)
      : _disk(msc, dev_addr, lun) {}

  /// Selects the device (kAnyDevice: the first one that is mounted).
  void begin(USBHostMSC &msc, uint8_t dev_addr = USBHostMscDisk::kAnyDevice,
             uint8_t lun = 0) {
    _disk.begin(msc, dev_addr, lun);
  }

  USBHostMscDisk &disk() { return _disk; }

  tinyext4::DSTATUS disk_initialize(tinyext4::BYTE pdrv) override {
    if (pdrv != 0 || !_disk.init()) {
      return gone();
    }
    return tinyext4::STA_CLEAR;
  }
  tinyext4::DSTATUS disk_status(tinyext4::BYTE pdrv) override {
    return (pdrv == 0 && _disk.ready()) ? tinyext4::STA_CLEAR : gone();
  }
  tinyext4::DRESULT disk_read(tinyext4::BYTE pdrv, tinyext4::BYTE *buff,
                              tinyext4::LBA_t sector,
                              tinyext4::UINT count) override {
    if (pdrv != 0 || !_disk.ready()) {
      return tinyext4::RES_NOTRDY;
    }
    return _disk.read(sector, buff, count) ? tinyext4::RES_OK
                                           : tinyext4::RES_ERROR;
  }
  tinyext4::DRESULT disk_write(tinyext4::BYTE pdrv, const tinyext4::BYTE *buff,
                               tinyext4::LBA_t sector,
                               tinyext4::UINT count) override {
    if (pdrv != 0 || !_disk.ready()) {
      return tinyext4::RES_NOTRDY;
    }
    return _disk.write(sector, buff, count) ? tinyext4::RES_OK
                                            : tinyext4::RES_ERROR;
  }
  tinyext4::DRESULT disk_ioctl(tinyext4::BYTE pdrv, tinyext4::ioctl_cmd_t cmd,
                               void *buff) override {
    if (pdrv != 0 || !_disk.ready()) {
      return tinyext4::RES_NOTRDY;
    }
    switch (cmd) {
    case tinyext4::CTRL_SYNC: // every write is already sent to the drive
      return tinyext4::RES_OK;
    case tinyext4::GET_SECTOR_COUNT:
      *(tinyext4::LBA_t *)buff = _disk.sectorCount();
      return tinyext4::RES_OK;
    case tinyext4::GET_SECTOR_SIZE:
      *(tinyext4::WORD *)buff = USBHostMscDisk::kSectorSize;
      return tinyext4::RES_OK;
    case tinyext4::GET_BLOCK_SIZE:
      *(tinyext4::DWORD *)buff = 1;
      return tinyext4::RES_OK;
    default:
      return tinyext4::RES_PARERR;
    }
  }

private:
  static tinyext4::DSTATUS gone() {
    return (tinyext4::DSTATUS)(tinyext4::STA_NOINIT | tinyext4::STA_NODISK);
  }
  USBHostMscDisk _disk;
};
