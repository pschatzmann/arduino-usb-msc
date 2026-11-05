#pragma once

#include <stdint.h>

/**
 * @brief Block storage: the common interface of everything that can be
 * exported over USB (USBDeviceMSC) or mounted as a filesystem
 * (USBMscFatFS::begin(USBMscBlockDevice&), USBMscExt4FS, ...).
 *
 * Implement it for a RAM disk, an SD card, SPI flash, an NBD export
 * (see USBMscNbdBlockDevice.h) or any other sector-addressable storage.
 * Block size is normally 512 -- the filesystem backends only support 512.
 * readBlocks()/writeBlocks() return true on success.
 *
 * This header has no dependencies, so it can be used in host and device
 * builds alike.
 */
class USBMscBlockDevice {
public:
  virtual ~USBMscBlockDevice() {}
  virtual uint32_t blockCount() = 0;
  virtual uint16_t blockSize() { return 512; }
  virtual bool readBlocks(uint32_t lba, void *buffer, uint32_t count) = 0;
  virtual bool writeBlocks(uint32_t lba, const void *buffer,
                           uint32_t count) = 0;
  /// Called when the PC asks to flush its write cache. Default: no-op.
  virtual void flush() {}
};
