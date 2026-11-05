#pragma once

#include <stdint.h>

// USBHOSTMSC_HOST_TEST: desktop unit tests (see docs/TESTING.md) swap the
// TinyUSB-backed USBHostMSC for an image-file stub with the same block API.
#if defined(USBHOSTMSC_HOST_TEST)
#include <USBHostMSC_host_test.h>
#else
#include "USBHostMSC.h"
#endif

/// One entry of an MBR partition table.
struct USBMscPartitionEntry {
  uint8_t type;         // MBR partition type byte (0x0B/0x0C FAT32, 0x83 Linux, ...)
  bool bootable;
  uint32_t lba_start;    // first sector, disk-relative
  uint32_t sector_count;
};

class USBMscPartition; // defined below

/**
 * @brief Reads and parses a classic MBR (DOS) partition table from LBA 0.
 *
 * GPT disks are detected (protective MBR, type 0xEE) but not parsed --
 * only MBR-partitioned and unpartitioned (superfloppy) media are supported.
 */
class USBMscPartitionTable {
public:
  /// Populates up to 4 entries; returns the number of non-empty entries
  /// found (0..4), or -1 if LBA 0 could not be read or has no valid MBR
  /// signature.
  static int8_t read(USBHostMSC &msc, uint8_t dev_addr,
                      USBMscPartitionEntry entries[4]);

  /// Binds `part` to the first partition whose type byte satisfies
  /// typeMatches. Falls back to the whole disk if there's no MBR, or no
  /// partition matches (so a superfloppy-formatted or misdetected drive
  /// still gets a usable binding to try mounting against).
  static void bindFirstMatching(USBHostMSC &msc, uint8_t dev_addr,
                                 bool (*typeMatches)(uint8_t type),
                                 USBMscPartition &part);
};

/// MBR partition type bytes commonly used for FAT12/16/32.
bool isFatPartitionType(uint8_t type);
/// MBR partition type byte used for native Linux filesystems (ext2/3/4).
bool isLinuxPartitionType(uint8_t type);

/**
 * @brief A block-device view of either a single MBR partition or a whole,
 * unpartitioned disk. LBA 0 of a USBMscPartition is the first sector of
 * the partition (or of the disk, in whole-disk mode).
 */
class USBMscPartition {
public:
  USBMscPartition() = default;

  /// Bind to a single partition described by entry.
  void begin(USBHostMSC &msc, uint8_t dev_addr,
             const USBMscPartitionEntry &entry);

  /// Bind to the whole disk (no partition offset).
  void begin(USBHostMSC &msc, uint8_t dev_addr);

  uint32_t blockSize() const;
  uint32_t blockCount() const { return _sector_count; }
  uint8_t deviceAddress() const { return _dev_addr; }

  bool readBlocks(uint32_t lba, void *buffer, uint16_t count);
  bool writeBlocks(uint32_t lba, const void *buffer, uint16_t count);

private:
  USBHostMSC *_msc = nullptr;
  uint8_t _dev_addr = 0;
  uint32_t _lba_start = 0;
  uint32_t _sector_count = 0;
};
