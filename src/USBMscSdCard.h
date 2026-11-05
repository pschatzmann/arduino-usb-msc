/*
 * USBMscSdCard -- exports an SD card (SPI or SDIO) over USB device mode.
 *
 * Wraps the raw sector access of SdFat's SdCard so the whole card -- with
 * whatever filesystem is on it (FAT, exFAT, ext2/3/4) -- can be handed to
 * USBDeviceMSC. The PC sees the card as a USB stick; the board never
 * interprets the filesystem.
 *
 * Needs the "SdFat" library (bundled with the RP2040 core; install
 * "SdFat - Adafruit Fork" or "SdFat" from the Library Manager on ESP32 etc.).
 * Use sd.cardBegin() -- not sd.begin() -- so cards without a FAT filesystem
 * (e.g. ext4) work too.
 */
#pragma once

#include <SdFat.h>

#include "USBDeviceMSC.h"

/**
 * @brief USBMscBlockDevice backed by an SdFat SD card (SPI or SDIO).
 *
 * Gives USBDeviceMSC raw sector access to the whole card, so the PC sees it as
 * a USB stick whatever filesystem it holds (FAT, exFAT, ext2/3/4). The board
 * never interprets the filesystem. Initialise the card with sd.cardBegin()
 * (not sd.begin()) so cards without a FAT filesystem work too.
 *
 * @code
 * SdFat sd;
 * sd.cardBegin(SdSpiConfig(CS_PIN, SHARED_SPI));
 * USBMscSdCard disk(sd.card());
 * USBDeviceMSC msc;
 * msc.begin(disk);
 * @endcode
 */
class USBMscSdCard : public USBMscBlockDevice {
public:
  /**
   * @brief Wrap an initialised card.
   * @param card Card to export, e.g. sd.card() after sd.cardBegin(). Must stay
   *             valid while exported; not owned. A null pointer yields an
   *             empty device (0 blocks, all I/O fails).
   */
  explicit USBMscSdCard(SdCard *card) : _card(card) {}

  /// Number of 512-byte sectors on the card (0 if no card).
  uint32_t blockCount() override { return _card ? _card->sectorCount() : 0; }
  /// Read `count` sectors starting at `lba`. @return true on success.
  bool readBlocks(uint32_t lba, void *buffer, uint32_t count) override {
    return _card && _card->readSectors(lba, (uint8_t *)buffer, count);
  }
  /// Write `count` sectors starting at `lba`. @return true on success.
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t count) override {
    return _card && _card->writeSectors(lba, (const uint8_t *)buffer, count);
  }
  /// Wait until pending card writes have completed.
  void flush() override {
    if (_card) _card->syncDevice();
  }

private:
  SdCard *_card;
};
