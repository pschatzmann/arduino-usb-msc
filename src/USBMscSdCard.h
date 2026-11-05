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

class USBMscSdCard : public USBMscBlockDevice {
public:
  /// `card` is the initialised card, e.g. sd.card() after sd.cardBegin().
  explicit USBMscSdCard(SdCard *card) : _card(card) {}

  uint32_t blockCount() override { return _card ? _card->sectorCount() : 0; }
  bool readBlocks(uint32_t lba, void *buffer, uint32_t count) override {
    return _card && _card->readSectors(lba, (uint8_t *)buffer, count);
  }
  bool writeBlocks(uint32_t lba, const void *buffer, uint32_t count) override {
    return _card && _card->writeSectors(lba, (const uint8_t *)buffer, count);
  }
  void flush() override {
    if (_card) _card->syncDevice();
  }

private:
  SdCard *_card;
};
