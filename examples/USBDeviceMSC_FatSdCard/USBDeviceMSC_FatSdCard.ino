/*
 * USBDeviceMSC_FatSdCard
 *
 * The board is a USB card reader: the SD card in its slot shows up on the
 * PC as a drive and can be read and written there. The board only serves
 * sectors (TinyFATFS's SPI SD driver), so the card can hold any filesystem.
 *
 * While the PC has the card mounted the MCU must not touch the files on it
 * (both sides cache and would corrupt each other). To access the card from
 * the sketch, call msc.mediaPresent(false) first ("eject"), mount it with
 * fatfs::SDClass, end() it, and call msc.mediaPresent(true) afterwards.
 *
 * Wiring: SD card on the default SPI pins, chip select = SD_CS below.
 * ESP32-S2/S3: Tools > USB Mode > "USB-OTG (TinyUSB)".
 * RP2040:      Tools > USB Stack > "Adafruit TinyUSB" (device stack).
 */
#include <SPI.h>

#include "USBDeviceMSC.h"
#include "fatfs.h"

#ifndef SD_CS
#define SD_CS SS
#endif

fatfs::ArduinoSpiIO card(SD_CS, SPI);
USBDeviceMSC msc;

void setup() {
  SPI.begin();
  msc.setID("Arduino", "SD Card", "1.0");
  if (!msc.begin(card)) { // call before Serial.begin() on RP2040
    while (true) delay(1000); // no card
  }
}

void loop() {}
