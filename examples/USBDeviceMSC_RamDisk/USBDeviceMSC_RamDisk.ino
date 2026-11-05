/*
 * USBDeviceMSC_RamDisk
 *
 * The board shows up on the PC as a small USB drive in RAM with a
 * HELLO.TXT on it. The sketch formats and fills the RAM disk with TinyFATFS
 * first, lets go of it (end()), and then exports the raw sectors with
 * USBDeviceMSC -- the PC and the MCU must never have the volume mounted at
 * the same time. The content is lost on reset.
 *
 * Any block driver of TinyFATFS or TinyEXT4 can be exported the same way.
 *
 * ESP32-S2/S3: Tools > USB Mode > "USB-OTG (TinyUSB)".
 * RP2040:      Tools > USB Stack > "Adafruit TinyUSB" (device stack).
 */
#include "USBDeviceMSC.h"
#include "fatfs.h"

RamIO ram(256); // 256 sectors * 512 bytes = 128 KB
USBDeviceMSC msc;

void setup() {
  SDClass fat;
  if (fat.begin(ram)) { // RamIO formats the volume when it is mounted
    File f = fat.open("/HELLO.TXT", FA_WRITE | FA_CREATE_ALWAYS);
    f.print("Hello from the board!\r\n");
    f.close();
    fat.end();
  }
  msc.setID("Arduino", "RAM Disk", "1.0");
  msc.begin(ram); // on RP2040: before Serial.begin()
}

void loop() {}
