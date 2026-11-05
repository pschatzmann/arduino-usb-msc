// Turns the board into a 64 KB USB flash drive backed by RAM, pre-loaded
// with one file. The PC can read, add and delete files; the content is lost
// on reset.
//
// ESP32-S2/S3: Tools > USB Mode > "USB-OTG (TinyUSB)".
// RP2040:      Tools > USB Stack > "Adafruit TinyUSB" (device stack).
#include "USBDeviceMSC.h"

USBMscRamDisk disk(128); // 128 blocks * 512 bytes
USBDeviceMSC msc;

void setup() {
  // Must run before the USB stack starts (before Serial.begin() on most cores).
  disk.begin(); // allocates and formats FAT12
  const char hello[] = "Hello from the board!\r\n";
  disk.addFile("HELLO.TXT", hello, sizeof(hello) - 1); // 8.3 names only

  msc.setID("Arduino", "RAM Disk", "1.0");
  msc.begin(disk);
}

void loop() {}
