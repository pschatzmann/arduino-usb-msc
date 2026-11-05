/*
 * USBDeviceMSC_FatSdCard
 *
 * The board is a USB card reader: the FAT/exFAT formatted SD card in its
 * slot shows up on the PC as a drive and can be read and written there.
 *
 * While the PC has the card mounted the MCU must not touch the files on it
 * (both sides cache and would corrupt each other). To access the card from
 * the sketch, call msc.mediaPresent(false) first ("eject"), and
 * msc.mediaPresent(true) afterwards.
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

SdFat sd;
USBMscSdCard *disk;
USBDeviceMSC msc;

void setup() {
  // cardBegin() only initialises the card; it does not mount a filesystem.
  if (!sd.cardBegin(SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(16)))) {
    while (true) delay(1000); // no card
  }
  static USBMscSdCard card(sd.card());
  disk = &card;

  msc.setID("Arduino", "SD Card", "1.0");
  msc.begin(*disk); // call before Serial.begin() on RP2040
}

void loop() {}
