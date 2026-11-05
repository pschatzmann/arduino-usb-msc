/*
 * USBDeviceMSC_Ext4SdCard
 *
 * The board is a USB card reader for an ext2/3/4 formatted SD card (format
 * it on Linux with `mkfs.ext4 /dev/sdX1`). The board never interprets the
 * filesystem, it only serves sectors, so any filesystem works.
 *
 * Linux (and macOS with ext4 support) can mount the drive; Windows does not
 * understand ext4 and offers to format it -- decline, that would erase the
 * card. To make sure nothing is erased by accident the export is read-only
 * by default; set EXT4_READ_WRITE to 1 to let a Linux PC write to it.
 *
 * Wiring: SD card on the default SPI pins, chip select = SD_CS below.
 * ESP32-S2/S3: Tools > USB Mode > "USB-OTG (TinyUSB)"; install SdFat.
 * RP2040:      Tools > USB Stack > "Adafruit TinyUSB" (device stack).
 */
#include <SPI.h>
#include <SdFat.h>

#include "USBDeviceMSC.h"
#include "USBMscSdCard.h"

#ifndef SD_CS
#define SD_CS SS
#endif
#define EXT4_READ_WRITE 0

SdFat sd;
USBDeviceMSC msc;

void setup() {
  // cardBegin() initialises the card without looking for a FAT filesystem
  // (sd.begin() would fail on ext4).
  if (!sd.cardBegin(SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(16)))) {
    while (true) delay(1000); // no card
  }
  static USBMscSdCard card(sd.card());

  msc.setID("Arduino", "ext4 SD Card", "1.0");
  msc.setWritable(EXT4_READ_WRITE);
  msc.begin(card); // call before Serial.begin() on RP2040
}

void loop() {}
