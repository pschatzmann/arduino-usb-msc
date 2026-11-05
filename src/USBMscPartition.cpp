#include "USBMsc_config.h"
#if USBHOSTMSC_COMPILE
#include "USBMscPartition.h"

#include <string.h>

static uint32_t rd_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

// Parses the MBR in `mbr` (one 512-byte sector); see read().
static int8_t parseMbr(const uint8_t *mbr, USBMscPartitionEntry entries[4]) {
  if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
    return -1; // no MBR signature
  }

  int8_t count = 0;
  for (uint8_t i = 0; i < 4; i++) {
    const uint8_t *e = mbr + 446 + (i * 16);
    uint8_t status = e[0];
    uint8_t type = e[4];
    uint32_t lba_start = rd_le32(e + 8);
    uint32_t sector_count = rd_le32(e + 12);

    if (type == 0x00) {
      continue; // empty entry
    }
    if (type == 0xEE) {
      continue; // GPT protective MBR entry -- GPT parsing not supported
    }

    entries[count].type = type;
    entries[count].bootable = (status == 0x80);
    entries[count].lba_start = lba_start;
    entries[count].sector_count = sector_count;
    count++;
  }
  return count;
}

int8_t USBMscPartitionTable::read(USBHostMSC &msc, uint8_t dev_addr,
                                   USBMscPartitionEntry entries[4]) {
  alignas(4) uint8_t mbr[512];
  if (msc.blockSize(dev_addr) > sizeof(mbr)) {
    return -1;
  }
  if (!msc.readBlocks(dev_addr, 0, mbr, 1)) {
    return -1;
  }
  return parseMbr(mbr, entries);
}

int8_t USBMscPartitionTable::read(USBMscBlockDevice &dev,
                                   USBMscPartitionEntry entries[4]) {
  alignas(4) uint8_t mbr[512];
  if (dev.blockSize() > sizeof(mbr)) {
    return -1;
  }
  if (!dev.readBlocks(0, mbr, 1)) {
    return -1;
  }
  return parseMbr(mbr, entries);
}

bool isFatPartitionType(uint8_t type) {
  // 0x07 is shared by exFAT and NTFS; NTFS is rejected later when FatFs
  // refuses to mount it.
  return type == 0x01 || type == 0x04 || type == 0x06 || type == 0x07 ||
         type == 0x0B || type == 0x0C || type == 0x0E;
}

bool isLinuxPartitionType(uint8_t type) { return type == 0x83; }

void USBMscPartitionTable::bindFirstMatching(USBHostMSC &msc, uint8_t dev_addr,
                                              bool (*typeMatches)(uint8_t),
                                              USBMscPartition &part) {
  USBMscPartitionEntry entries[4];
  int8_t n = read(msc, dev_addr, entries);

  if (n > 0) {
    for (int8_t i = 0; i < n; i++) {
      if (typeMatches(entries[i].type)) {
        part.begin(msc, dev_addr, entries[i]);
        return;
      }
    }
  }
  part.begin(msc, dev_addr); // no MBR, or no matching partition: whole disk
}

void USBMscPartitionTable::bindFirstMatching(USBMscBlockDevice &dev,
                                              bool (*typeMatches)(uint8_t),
                                              USBMscPartition &part) {
  USBMscPartitionEntry entries[4];
  int8_t n = read(dev, entries);

  if (n > 0) {
    for (int8_t i = 0; i < n; i++) {
      if (typeMatches(entries[i].type)) {
        part.begin(dev, entries[i]);
        return;
      }
    }
  }
  part.begin(dev); // no MBR, or no matching partition: whole device
}

void USBMscPartition::begin(USBHostMSC &msc, uint8_t dev_addr,
                             const USBMscPartitionEntry &entry) {
  _msc = &msc;
  _dev = nullptr;
  _dev_addr = dev_addr;
  _lba_start = entry.lba_start;
  _sector_count = entry.sector_count;
  // Don't trust the table: clamp the partition to the physical disk.
  uint32_t disk_blocks = msc.blockCount(dev_addr);
  if (_lba_start >= disk_blocks) {
    _sector_count = 0;
  } else if (_sector_count > disk_blocks - _lba_start) {
    _sector_count = disk_blocks - _lba_start;
  }
}

void USBMscPartition::begin(USBHostMSC &msc, uint8_t dev_addr) {
  _msc = &msc;
  _dev = nullptr;
  _dev_addr = dev_addr;
  _lba_start = 0;
  _sector_count = msc.blockCount(dev_addr);
}

void USBMscPartition::begin(USBMscBlockDevice &dev,
                             const USBMscPartitionEntry &entry) {
  _msc = nullptr;
  _dev = &dev;
  _dev_addr = 0;
  _lba_start = entry.lba_start;
  _sector_count = entry.sector_count;
  // Don't trust the table: clamp the partition to the device.
  uint32_t disk_blocks = dev.blockCount();
  if (_lba_start >= disk_blocks) {
    _sector_count = 0;
  } else if (_sector_count > disk_blocks - _lba_start) {
    _sector_count = disk_blocks - _lba_start;
  }
}

void USBMscPartition::begin(USBMscBlockDevice &dev) {
  _msc = nullptr;
  _dev = &dev;
  _dev_addr = 0;
  _lba_start = 0;
  _sector_count = dev.blockCount();
}

uint32_t USBMscPartition::blockSize() const {
  if (_dev) {
    return _dev->blockSize();
  }
  return _msc ? _msc->blockSize(_dev_addr) : 0;
}

bool USBMscPartition::readBlocks(uint32_t lba, void *buffer, uint16_t count) {
  if ((!_msc && !_dev) || (uint64_t)lba + count > _sector_count) {
    return false;
  }
  if (_dev) {
    return _dev->readBlocks(_lba_start + lba, buffer, count);
  }
  return _msc->readBlocks(_dev_addr, _lba_start + lba, buffer, count);
}

bool USBMscPartition::writeBlocks(uint32_t lba, const void *buffer,
                                   uint16_t count) {
  if ((!_msc && !_dev) || (uint64_t)lba + count > _sector_count) {
    return false;
  }
  if (_dev) {
    return _dev->writeBlocks(_lba_start + lba, buffer, count);
  }
  return _msc->writeBlocks(_dev_addr, _lba_start + lba, buffer, count);
}

#endif // USBHOSTMSC_COMPILE
